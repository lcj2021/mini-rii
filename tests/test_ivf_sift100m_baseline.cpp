#include <random>
#include <iostream>
#include <numeric>
#include <unordered_set>

#include "binary_io.hpp"
#include "index_ivf.hpp"
#include "quantizer.hpp"
#include "util.hpp"

size_t D;              // dimension of the vectors to index
size_t nb;       // size of the database we plan to index
size_t nt = 1000'000;         // make a set of nt training vectors in the unit cube (could be the database)
size_t nq = 1'000;
int ncentroids = 10'000;

std::string suffix = "nt" + ToStringWithUnits(nt) 
                    + "_kc" + std::to_string(ncentroids);

std::string index_path = std::string("/dk/anns/index/sift100m/")
                    + suffix;
std::string db_path = "/dk/anns/dataset/sift100m";
std::string query_path = "/dk/anns/query/sift100m";

int main(int argc, char* argv[]) {
    assert(argc == 2);
    std::vector<float> database;
    std::tie(nb, D) = LoadFromFileBinary<uint8_t>(database, db_path + "/base.bvecs");

    std::vector<float> query;
    LoadFromFileBinary<uint8_t>(query, query_path + "/query.bvecs");

    std::vector<int> gt;
    auto [n_gt, d_gt] = LoadFromFileBinary<int>(gt, query_path + "/gt.ivecs");

    std::vector<float> gt_dist;
    LoadFromFileBinary<float>(gt_dist, query_path + "/dist.fvecs");

    int nprobe = std::atoi(argv[1]);

    toy::IVFConfig cfg(
        nb, D, nb, 
        ncentroids, 1, D, 
        index_path, db_path
    );
    toy::IndexIVF index(cfg, nq, true);
    // index.Train(database, 123, nt);
    // index.WriteIndex(index_path);
    index.LoadIndex(index_path);
    index.Populate(database);

    puts("Index find kNN!");
    // Recall@k
    int k = 100;
    std::vector<std::vector<size_t>> nnid(nq, std::vector<size_t>(k));
    std::vector<std::vector<float>> dist(nq, std::vector<float>(k));
    Timer timer_query;
    timer_query.Start();
    size_t total_searched_cnt = 0;

    #pragma omp parallel for reduction(+ : total_searched_cnt)
    for (size_t q = 0; q < nq; ++q) {
        size_t searched_cnt;
        index.QueryBaseline(
            std::vector<float>(query.begin() + q * D, query.begin() + (q + 1) * D), 
            nnid[q], dist[q], searched_cnt, 
             k, nb, q, nprobe
        );
        total_searched_cnt += searched_cnt;
    }
    timer_query.Stop();
    std::cout << timer_query.GetTime() << " seconds.\n";


    int n_ok = 0;
    for (int q = 0; q < nq; ++q) {
        std::unordered_set<int> S(gt.begin() + q * d_gt, gt.begin() + q * d_gt + k);
        for (int i = 0; i < k; ++i)
            if (S.count(nnid[q][i]))
                n_ok++;
    }
    std::cout << "Recall@" << k << ": " << (double)n_ok / (nq * k) << '\n';
    std::cout << "avg_searched_cnt: " << (double)total_searched_cnt / nq << '\n';
    printf("kc%d, W%d\n", ncentroids, nprobe);

    return 0;
}