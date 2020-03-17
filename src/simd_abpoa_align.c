#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abpoa_align.h"
#include "simd_instruction.h"
#include "utils.h"

typedef struct {
    const int reg_n, bits_n, log_num, num_of_value, size;
    int inf_min; // based on penalty of mismatch and GAP_OE1
} SIMD_para_t;

#define SIMDShiftOneNi8  1
#define SIMDShiftOneNi16 2
#define SIMDShiftOneNi32 4
#define SIMDShiftOneNi64 8

#ifdef __AVX512F__
SIMD_para_t _simd_p8  = {512,  8, 6, 64, 64, -1};
SIMD_para_t _simd_p16 = {512, 16, 5, 32, 64, -1};
SIMD_para_t _simd_p32 = {512, 32, 4, 16, 64, -1};
SIMD_para_t _simd_p64 = {512, 64, 3,  8, 64, -1};
#define SIMDTotalBytes 64
#elif defined(__AVX2__)
SIMD_para_t _simd_p8  = {256,  8, 5, 32, 32, -1};
SIMD_para_t _simd_p16 = {256, 16, 4, 16, 32, -1};
SIMD_para_t _simd_p32 = {256, 32, 3,  8, 32, -1};
SIMD_para_t _simd_p64 = {256, 64, 2,  4, 32, -1};
#define SIMDTotalBytes 32
#else
SIMD_para_t _simd_p8  = {128,  8, 4, 16, 16, -1};
SIMD_para_t _simd_p16 = {128, 16, 3,  8, 16, -1};
SIMD_para_t _simd_p32 = {128, 32, 2,  4, 16, -1};
SIMD_para_t _simd_p64 = {128, 64, 1,  2, 16, -1};
#define SIMDTotalBytes 16
#endif

#define print_simd(s, str, score_t) {                                   \
    int _i; score_t *_a = (score_t*)s;                                  \
    printf("%s\t", str);                                                \
    for (_i = 0; _i < SIMDTotalBytes / (int)sizeof(score_t); ++_i) {    \
        printf("%d\t", _a[_i]);                                         \
    } printf("\n");                                                     \
}

#define simd_abpoa_print_ag_matrix(score_t) {                   \
    for (j = 0; j <= matrix_row_n-2; ++j) {	                    \
        printf("index: %d\t", j);	                            \
        dp_h = DP_HE + (j << 1) * dp_sn; dp_e = dp_h + dp_sn;	\
        _dp_h = (score_t*)dp_h, _dp_e = (score_t*)dp_e;	        \
        for (i = dp_beg[j]; i <= dp_end[j]; ++i) {	            \
            printf("%d:(%d,%d)\t", i, _dp_h[i], _dp_e[i]);	    \
        } printf("\n");	                                        \
    }	                                                        \
}

#define debug_simd_abpoa_print_cg_matrix_row(str, score_t, index_i) {                           \
    score_t *_dp_h = (score_t*)dp_h, *_dp_e1 = (score_t*)dp_e1;                                 \
    score_t *_dp_e2 = (score_t*)dp_e2, *_dp_f = (score_t*)dp_f, *_dp_f2 = (score_t*)dp_f2;      \
    printf("%s\tindex: %d\t", str, index_i);	                                                \
    for (i = dp_beg[index_i]; i <= (dp_end[index_i]/16+1)*16-1; ++i) {	                        \
        printf("%d:(%d,%d,%d,%d,%d)\t", i, _dp_h[i], _dp_e1[i], _dp_e2[i], _dp_f[i], _dp_f2[i]);\
    } printf("\n");	                                                                            \
}

#define simd_abpoa_print_lg_matrix_row(id, score_t, index_i) {          \
    printf("%d\tindex: %d\t", id, index_i);	                            \
    for (i = dp_beg[index_i]; i <= (dp_end[index_i]/16+1)*16-1; ++i) {	\
        printf("%d:(%d)\t", i, _dp_h[i]);	                            \
    } printf("\n");	                                                    \
}

#define simd_abpoa_print_cg_matrix_row(score_t, index_i) {                                          \
    printf("index: %d\t", index_i);	                                                                \
    for (i = dp_beg[index_i]; i <= (dp_end[index_i]/16+1)*16-1; ++i) {	                            \
        printf("%d:(%d,%d,%d,%d,%d)\t", i, _dp_h[i], _dp_e1[i], _dp_e2[i], _dp_f[i], _dp_f2[i]);	\
    } printf("\n");	                                                                                \
}

#define simd_abpoa_print_cg_matrix(score_t) {                                                   \
    for (j = 0; j <= matrix_row_n-2; ++j) {	                                                    \
        printf("index: %d\t", j);	                                                            \
        dp_h = DP_H2E + j * 3 * dp_sn; dp_e1 = dp_h + dp_sn; dp_e2 = dp_e1 + dp_sn;	            \
        score_t *_dp_h = (score_t*)dp_h, *_dp_e1 = (score_t*)dp_e1, *_dp_e2 = (score_t*)dp_e2;	\
        for (i = dp_beg[j]; i <= dp_end[j]; ++i) {                                              \
        /*for (i = 0; i <= qlen; ++i) {  */                                                     \
            printf("%d:(%d,%d,%d)\t", i, _dp_h[i], _dp_e1[i], _dp_e2[i]);	                    \
        } printf("\n");	                                                                        \
    }	                                                                                        \
}

#define simd_abpoa_print_lg_matrix(score_t) {       \
    for (j = 0; j <= graph->node_n-2; ++j) {	    \
        printf("index: %d\t", j);	                \
        dp_h = DP_H + j * dp_sn;	                \
        _dp_h = (score_t*)dp_h;	                    \
        for (i = dp_beg[j]; i <= dp_end[j]; ++i) {	\
            printf("%d:(%d)\t", i, _dp_h[i]);	    \
        } printf("\n");	                            \
    }	                                            \
}

/* min/max_rank: min/max of max column index for each row, based on the pre_nodes' DP score */
/* === workflow of alignment === */
/* a. global:
 * (1) alloc mem 
 * (2) init for first row
 * (3) DP for each row
 * (3.2) if use_ada, update min/max_rank 
 * (4) find best_i/j, backtrack
 * b. extend:
 * (1) alloc mem
 * (2) init for first row
 * (3) DP for each row
 * (3.2) find max of current row
 * (3.3) z-drop, set_max_score
 * (3.4) if use_ada, update min/max_rank
 */

// backtrack order:
// Match/Mismatch, Deletion, Insertion
#define simd_abpoa_lg_backtrack(score_t, DP_H, dp_sn, m, mat, gap_e, pre_index, pre_n, start_i, start_j,    \
        best_i, best_j, qlen, graph, query, res) {                                                          \
    int i, j, k, pre_i, n_c = 0, s, m_c = 0, hit, id, _start_i, _start_j;                                   \
    SIMDi *dp_h, *pre_dp_h; score_t *_dp_h=NULL, *_pre_dp_h; abpoa_cigar_t *cigar = 0;                      \
    i = best_i, j = best_j, _start_i = best_i, _start_j = best_j;                                           \
    id = abpoa_graph_index_to_node_id(graph, i);                                                            \
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);   \
    dp_h = DP_H + i * dp_sn; _dp_h = (score_t*)dp_h;                                                        \
    while (i > start_i && j > start_j) {                                                                    \
        _start_i = i, _start_j = j;                                                                         \
        int *pre_index_i = pre_index[i];                                                                    \
        s = mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            \
        for (k = 0; k < pre_n[i]; ++k) {                                                                    \
            pre_i = pre_index_i[k];                                                                         \
            if (j-1 < dp_beg[pre_i] || j-1 > dp_end[pre_i]) continue;                                       \
            pre_dp_h = DP_H + pre_i * dp_sn; _pre_dp_h = (score_t*)pre_dp_h;                                \
            if (_pre_dp_h[j-1] + s == _dp_h[j]) { /* match/mismatch */                                      \
                cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                      \
                i = pre_i; --j; hit = 1; id = abpoa_graph_index_to_node_id(graph, i);                       \
                dp_h = DP_H + i * dp_sn; _dp_h = (score_t*)dp_h;                                            \
                ++res->n_aln_bases; res->n_matched_bases += s == mat[0] ? 1 : 0;                            \
                break;                                                                                      \
            }                                                                                               \
        }                                                                                                   \
        if (hit == 0) {                                                                                     \
            for (k = 0; k < pre_n[i]; ++k) {                                                                \
                pre_i = pre_index_i[k];                                                                     \
                if (j < dp_beg[pre_i] || j > dp_end[pre_i]) continue;                                       \
                pre_dp_h = DP_H + pre_i * dp_sn; _pre_dp_h = (score_t*)pre_dp_h;                            \
                if (_pre_dp_h[j] - gap_e == _dp_h[j]) { /* deletion */                                      \
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                    \
                    i = pre_i; hit = 1; id = abpoa_graph_index_to_node_id(graph, i);                        \
                    dp_h = DP_H + i * dp_sn; _dp_h = (score_t*)dp_h;                                        \
                    break;                                                                                  \
                }                                                                                           \
            }                                                                                               \
        }                                                                                                   \
        if (hit == 0) { /* insertion */                                                                     \
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); j--;                       \
            ++res->n_aln_bases;                                                                             \
        }                                                                                                   \
    }                                                                                                       \
    if (j > start_j) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j-start_j, -1, j-1);     \
    /* reverse cigar */                                                                                     \
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                           \
    res->n_cigar = n_c;                                                                                     \
    res->node_e = best_i, res->query_e = best_j-1; /* 0-based */                                            \
    res->node_s = _start_i, res->query_s = _start_j-1;                                                      \
    /*abpoa_print_cigar(n_c, *graph_cigar, graph);*/                                                        \
}

#define simd_abpoa_ag_backtrack(score_t, DP_HE, dp_sn, m, mat, gap_e, pre_index, pre_n,                     \
        start_i, start_j, best_i, best_j, qlen, graph, query, res) {                                        \
    int i, j, k, pre_i, n_c = 0, s, m_c = 0, id, hit, cur_op = ABPOA_ALL_OP, _start_i, _start_j;            \
    SIMDi *dp_h, *pre_dp_h, *pre_dp_e;                                                                      \
    score_t *_dp_h=NULL, *_pre_dp_h, *_pre_dp_e; abpoa_cigar_t *cigar = 0;                                  \
    i = best_i, j = best_j; _start_i = best_i, _start_j = best_j;                                           \
    id = abpoa_graph_index_to_node_id(graph, i);                                                            \
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);   \
    dp_h = DP_HE + dp_sn * (i << 1); _dp_h = (score_t*)dp_h;                                                \
    while (i > start_i && j > start_j) {                                                                    \
        _start_i = i, _start_j = j;                                                                         \
        int *pre_index_i = pre_index[i];                                                                    \
        s = mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            \
        if (cur_op & ABPOA_M_OP) {                                                                          \
            for (k = 0; k < pre_n[i]; ++k) {                                                                \
                pre_i = pre_index_i[k];                                                                     \
                if (j-1 < dp_beg[pre_i] || j-1 > dp_end[pre_i]) continue;                                   \
                /* match/mismatch */                                                                        \
                pre_dp_h = DP_HE + dp_sn * (pre_i << 1); _pre_dp_h = (score_t*)pre_dp_h;                    \
                if (_pre_dp_h[j-1] + s == _dp_h[j]) {                                                       \
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                  \
                    i = pre_i; --j; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                   \
                    dp_h = DP_HE + dp_sn * (i << 1); _dp_h = (score_t*)dp_h;                                \
                    cur_op = ABPOA_ALL_OP;                                                                  \
                    ++res->n_aln_bases; res->n_matched_bases += s == mat[0] ? 1 : 0;                        \
                    break;                                                                                  \
                }                                                                                           \
            }                                                                                               \
        }                                                                                                   \
        if (hit == 0 && cur_op & ABPOA_E1_OP) {                                                             \
            for (k = 0; k < pre_n[i]; ++k) {                                                                \
                pre_i = pre_index_i[k];                                                                     \
                if (j < dp_beg[pre_i] || j > dp_end[pre_i]) continue;                                       \
                pre_dp_e = DP_HE + dp_sn * ((pre_i << 1) + 1); _pre_dp_e = (score_t*)pre_dp_e;              \
                if (_pre_dp_e[j] == _dp_h[j]) { /* deletion */                                              \
                    pre_dp_h = DP_HE + dp_sn * (pre_i << 1); _pre_dp_h = (score_t*)pre_dp_h;                \
                    if (_pre_dp_h[j] - gap_e == _pre_dp_e[j]) cur_op = ABPOA_E1_OP;                         \
                    else cur_op = ABPOA_M_OP;                                                               \
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                    \
                    i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                        \
                    dp_h = DP_HE + dp_sn * (i << 1); _dp_h = (score_t*)dp_h;                                \
                    break;                                                                                  \
                }                                                                                           \
            }                                                                                               \
        }                                                                                                   \
        if (hit == 0) { /* insertion */                                                                     \
            cur_op = ABPOA_M_OP;                                                                            \
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); --j;                       \
            ++res->n_aln_bases;                                                                             \
        }                                                                                                   \
    }                                                                                                       \
    if (j > start_j) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j-start_j, -1, j-1);     \
    /* reverse cigar */                                                                                     \
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                           \
    res->n_cigar = n_c;                                                                                     \
    res->node_e = best_i, res->query_e = best_j-1; /* 0-based */                                            \
    res->node_s = _start_i, res->query_s = _start_j-1;                                                      \
    /*abpoa_print_cigar(n_c, *graph_cigar, graph);*/                                                        \
}

#define simd_abpoa_cg_backtrack(score_t, DP_H2E, dp_sn, m, mat, gap_ext1, gap_ext2, gap_oe1,                \
        pre_index, pre_n, start_i, start_j, best_i, best_j, qlen, graph, query, res) {                      \
    int i, j, k, pre_i, n_c = 0, s, m_c = 0, id, hit, cur_op = ABPOA_ALL_OP, _start_i, _start_j;            \
    SIMDi *dp_h, *pre_dp_h, *pre_dp_e1, *pre_dp_e2;                                                         \
    score_t *_dp_h=NULL, *_pre_dp_h, *_pre_dp_e1, *_pre_dp_e2; abpoa_cigar_t *cigar = 0;                    \
    i = best_i, j = best_j, _start_i = best_i, _start_j = best_j;                                           \
    id = abpoa_graph_index_to_node_id(graph, i);                                                            \
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);   \
    dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (score_t*)dp_h;                                                \
    while (i > start_i && j > start_j) {                                                                    \
        _start_i = i, _start_j = j;                                                                         \
       /* fprintf(stderr, "ij: %d, %d\n", i, j); */                                                         \
        int *pre_index_i = pre_index[i];                                                                    \
        s = mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            \
        if (cur_op & ABPOA_M_OP) {                                                                          \
            for (k = 0; k < pre_n[i]; ++k) {                                                                \
                pre_i = pre_index_i[k];                                                                     \
                if (j-1 < dp_beg[pre_i] || j-1 > dp_end[pre_i]) continue;                                   \
                pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (score_t*)pre_dp_h;                    \
                if (_pre_dp_h[j-1] + s == _dp_h[j]) { /* match/mismatch */                                  \
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                  \
                    i = pre_i; --j; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                   \
                    dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (score_t*)dp_h;                                \
                    cur_op = ABPOA_ALL_OP;                                                                  \
                    ++res->n_aln_bases; res->n_matched_bases += s == mat[0] ? 1 : 0;                        \
                    break;                                                                                  \
                }                                                                                           \
            }                                                                                               \
        }                                                                                                   \
        if (hit == 0 && cur_op & ABPOA_E_OP) {                                                              \
            for (k = 0; k < pre_n[i]; ++k) {                                                                \
                pre_i = pre_index_i[k];                                                                     \
                if (j < dp_beg[pre_i] || j > dp_end[pre_i]) continue;                                       \
                if (cur_op & ABPOA_E1_OP) {                                                                 \
                    pre_dp_e1 = DP_H2E + dp_sn * ((pre_i * 3) + 1); _pre_dp_e1 = (score_t*)pre_dp_e1;       \
                    if (_pre_dp_e1[j] == _dp_h[j]) { /* deletion */                                         \
                        pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (score_t*)pre_dp_h;            \
                        if (_pre_dp_h[j] - gap_ext1 == _pre_dp_e1[j]) cur_op = ABPOA_E1_OP;                 \
                        else cur_op = ABPOA_M_OP;                                                           \
                        cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                \
                        i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                    \
                        dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (score_t*)dp_h;                            \
                        break;                                                                              \
                    }                                                                                       \
                }                                                                                           \
                if (cur_op & ABPOA_E2_OP) {                                                                 \
                    pre_dp_e2 = DP_H2E + dp_sn * ((pre_i * 3) + 2); _pre_dp_e2 = (score_t*)pre_dp_e2;       \
                    if (_pre_dp_e2[j] == _dp_h[j]) { /* deletion */                                         \
                        pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (score_t*)pre_dp_h;            \
                        if (_pre_dp_h[j] - gap_ext2 == _pre_dp_e2[j]) cur_op = ABPOA_E_OP;                  \
                        else cur_op = ABPOA_M_OP;                                                           \
                        cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                \
                        i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                    \
                        dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (score_t*)dp_h;                            \
                        break;                                                                              \
                    }                                                                                       \
                }                                                                                           \
            }                                                                                               \
        }                                                                                                   \
        if (hit == 0) { /* insertion */                                                                     \
            cur_op = ABPOA_M_OP;                                                                            \
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); --j;                       \
            ++res->n_aln_bases;                                                                             \
        }                                                                                                   \
      /*printf("%d, %d, %d\n", i, j, cur_op);*/                                                             \
    }                                                                                                       \
    if (j > start_j) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j-start_j, -1, j-1);     \
    /* reverse cigar */                                                                                     \
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                           \
    res->n_cigar = n_c;                                                                                     \
    res->node_e = best_i, res->query_e = best_j-1; /* 0-based */                                            \
    res->node_s = _start_i, res->query_s = _start_j-1;                                                      \
    /*abpoa_print_cigar(n_c, *graph_cigar, graph);*/                                                        \
}

// simd_abpoa_va
// simd_abpoa_ag_only_var
// sim_abpoa_init_var
#define simd_abpoa_var(score_t, sp, SIMDSetOne, SIMDShiftOneN)                                      \
    /* int tot_dp_sn = 0; */                                                                        \
    abpoa_graph_t *graph = ab->abg; abpoa_simd_matrix_t *abm = ab->abm;                             \
    int matrix_row_n = graph->node_n, matrix_col_n = qlen + 1;                                      \
    int **pre_index, *pre_n, pre_i;                                                                 \
    int i, j, k, *dp_beg, *dp_beg_sn, *dp_end, *dp_end_sn, node_id, index_i;                        \
    int beg, end, beg_sn, end_sn, _beg_sn, _end_sn, pre_beg_sn, pre_end, sn_i;                      \
    int pn, log_n, size, qp_sn, dp_sn; /* pn: value per SIMDi, qp_sn/dp_sn/d_sn: segmented length*/ \
    SIMDi *dp_h, *pre_dp_h, *qp, *qi=NULL;                                                          \
    score_t *_dp_h=NULL, *_qi, best_score = sp.inf_min, inf_min = sp.inf_min;                       \
    int *mat = abpt->mat, best_i = 0, best_j = 0, max, max_i; score_t gap_ext1 = abpt->gap_ext1;    \
    int w = abpt->bw < 0 ? qlen : abpt->bw; /* when w < 0, do whole global */                       \
    SIMDi zero = SIMDSetZeroi(), SIMD_MIN_INF = SIMDSetOne(inf_min);                                \
    pn = sp.num_of_value; qp_sn = dp_sn = (matrix_col_n + pn - 1) / pn;                             \
    log_n = sp.log_num, size = sp.size; qp = abm->s_mem;                                            \
    int set_num; SIMDi *PRE_MASK, *SUF_MIN, *PRE_MIN;                                               \
    PRE_MASK = (SIMDi*)SIMDMalloc((pn+1) * size, size);                                             \
    SUF_MIN = (SIMDi*)SIMDMalloc((pn+1) * size, size);                                              \
    PRE_MIN = (SIMDi*)SIMDMalloc(pn * size, size);                                                  \
    for (i = 0; i < pn; ++i) {                                                                      \
        score_t *pre_mask = (score_t*)(PRE_MASK+i);                                                 \
        for (j = 0; j <= i; ++j) pre_mask[j] = -1;                                                  \
        for (j = i+1; j < pn; ++j) pre_mask[j] = 0;                                                 \
    } PRE_MASK[pn] = PRE_MASK[pn-1];                                                                \
    SUF_MIN[0] = SIMDShiftLeft(SIMD_MIN_INF, SIMDShiftOneN);                                        \
    for (i = 1; i < pn; ++i)                                                                        \
        SUF_MIN[i] = SIMDShiftLeft(SUF_MIN[i-1], SIMDShiftOneN); SUF_MIN[pn] = SUF_MIN[pn-1];       \
    for (i = 1; i < pn; ++i) {                                                                      \
        score_t *pre_min = (score_t*)(PRE_MIN + i);                                                 \
        for (j = 0; j < i; ++j) pre_min[j] = inf_min;                                               \
        for (j = i; j < pn; ++j) pre_min[j] = 0;                                                    \
    }

#define simd_abpoa_lg_only_var(score_t, SIMDSetOne, SIMDAdd)    \
    SIMDi *DP_H = qp + qp_sn * abpt->m; SIMDi *dp_f = DP_H;     \
    SIMDi GAP_E1 = SIMDSetOne(gap_ext1);                        \
    SIMDi *GAP_E1S =  (SIMDi*)SIMDMalloc(log_n * size, size);   \
    GAP_E1S[0] = GAP_E1;                                        \
    for (i = 1; i < log_n; ++i) {                               \
        GAP_E1S[i] = SIMDAdd(GAP_E1S[i-1], GAP_E1S[i-1]);       \
    }

#define simd_abpoa_ag_only_var(score_t, SIMDSetOne, SIMDAdd)                                            \
    score_t *_dp_e, gap_open1 = abpt->gap_open1, gap_oe = abpt->gap_open1 + abpt->gap_ext1;             \
    SIMDi *DP_HE, *dp_e, *pre_dp_e, *dp_f; int pre_end_sn;                                              \
    DP_HE = qp + qp_sn * abpt->m; dp_f = DP_HE + ((dp_sn * matrix_row_n) << 1);                         \
    SIMDi GAP_O1 = SIMDSetOne(gap_open1), GAP_E1 = SIMDSetOne(gap_ext1), GAP_OE1 = SIMDSetOne(gap_oe);  \
    SIMDi *GAP_E1S =  (SIMDi*)SIMDMalloc(log_n * size, size);  GAP_E1S[0] = GAP_E1;                     \
    for (i = 1; i < log_n; ++i) {                                                                       \
        GAP_E1S[i] = SIMDAdd(GAP_E1S[i-1], GAP_E1S[i-1]);                                               \
    }

#define simd_abpoa_cg_only_var(score_t, SIMDSetOne, SIMDAdd)                                                        \
    score_t *_dp_e1, *_dp_e2, gap_open1 = abpt->gap_open1, gap_oe1 = gap_open1 + gap_ext1;                          \
    score_t gap_open2 = abpt->gap_open2, gap_ext2 = abpt->gap_ext2, gap_oe2 = gap_open2 + gap_ext2;                 \
    SIMDi *DP_H2E, *dp_e1, *dp_e2, *dp_f, *dp_f2, *pre_dp_e1, *pre_dp_e2; int pre_end_sn;                           \
    SIMDi GAP_O1 = SIMDSetOne(gap_open1), GAP_O2 = SIMDSetOne(gap_open2);                                           \
    SIMDi GAP_E1 = SIMDSetOne(gap_ext1), GAP_E2 = SIMDSetOne(gap_ext2);                                             \
    SIMDi GAP_OE1 = SIMDSetOne(gap_oe1), GAP_OE2 = SIMDSetOne(gap_oe2);                                             \
    DP_H2E = qp + qp_sn * abpt->m; dp_f2 = DP_H2E + dp_sn * matrix_row_n * 3; dp_f = dp_f2 + dp_sn;                 \
    SIMDi *GAP_E1S =  (SIMDi*)SIMDMalloc(log_n * size, size), *GAP_E2S =  (SIMDi*)SIMDMalloc(log_n * size, size);   \
    GAP_E1S[0] = GAP_E1; GAP_E2S[0] = GAP_E2;                                                                       \
    for (i = 1; i < log_n; ++i) {                                                                                   \
        GAP_E1S[i] = SIMDAdd(GAP_E1S[i-1], GAP_E1S[i-1]);                                                           \
        GAP_E2S[i] = SIMDAdd(GAP_E2S[i-1], GAP_E2S[i-1]);                                                           \
    }

#define simd_abpoa_init_var(score_t) {                                                                  \
    /* generate the query profile */                                                                    \
    for (i = 0; i < qp_sn * abpt->m; ++i) qp[i] = SIMD_MIN_INF;                                         \
    for (k = 0; k < abpt->m; ++k) { /* SIMD parallelization */                                          \
        int *p = &mat[k * abpt->m];                                                                     \
        score_t *_qp = (score_t*)(qp + k * qp_sn); _qp[0] = 0;                                          \
        for (j = 0; j < qlen; ++j) _qp[j+1] = (score_t)p[query[j]];                                     \
        for (j = qlen+1; j < qp_sn * pn; ++j) _qp[j] = 0;                                               \
    }                                                                                                   \
    if (abpt->bw >= 0 || abpt->align_mode == ABPOA_EXTEND_MODE) { /* query index */                     \
        qi = dp_f + dp_sn; _qi = (score_t*)qi;                                                          \
        for (i = 0; i <= qlen; ++i) _qi[i] = i;                                                         \
        for (i = qlen+1; i < (qlen/pn+1) * pn; ++i) _qi[i] = -1;                                        \
    }                                                                                                   \
    /* for backtrack */                                                                                 \
    dp_beg = abm->dp_beg, dp_end = abm->dp_end, dp_beg_sn = abm->dp_beg_sn, dp_end_sn = abm->dp_end_sn; \
    /* index of pre-node */                                                                             \
    pre_index = (int**)_err_malloc(graph->node_n * sizeof(int*));                                       \
    pre_n = (int*)_err_malloc(graph->node_n * sizeof(int*));                                            \
    for (i = 0; i < graph->node_n; ++i) {                                                               \
        node_id = abpoa_graph_index_to_node_id(graph, i); /* i: node index */                           \
        pre_n[i] = graph->node[node_id].in_edge_n;                                                      \
        pre_index[i] = (int*)_err_malloc(pre_n[i] * sizeof(int));                                       \
        for (j = 0; j < pre_n[i]; ++j) {                                                                \
            pre_index[i][j] = abpoa_graph_node_id_to_index(graph, graph->node[node_id].in_id[j]);       \
        }                                                                                               \
    }                                                                                                   \
}

#define simd_abpoa_free_var {                                                               \
    for (i = 0; i < graph->node_n; ++i) free(pre_index[i]); free(pre_index); free(pre_n);	\
    free(PRE_MASK); free(SUF_MIN); free(PRE_MIN);                                           \
}	                                                                                        \

#define simd_abpoa_lg_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd)  \
    simd_abpoa_var(score_t, sp, SIMDSetOne, SIMDShiftOneN);                 \
    simd_abpoa_lg_only_var(score_t, SIMDSetOne, SIMDAdd);                   \
    simd_abpoa_init_var(score_t);               

#define simd_abpoa_ag_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd)  \
    simd_abpoa_var(score_t, sp, SIMDSetOne, SIMDShiftOneN);                 \
    simd_abpoa_ag_only_var(score_t, SIMDSetOne, SIMDAdd);                   \
    simd_abpoa_init_var(score_t);               

#define simd_abpoa_cg_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd)  \
    simd_abpoa_var(score_t, sp, SIMDSetOne, SIMDShiftOneN);                 \
    simd_abpoa_cg_only_var(score_t, SIMDSetOne, SIMDAdd);                   \
    simd_abpoa_init_var(score_t);

#define simd_abpoa_lg_first_row {                                                                                   \
    /* fill the first row */	                                                                                    \
    if (abpt->bw >= 0) {                                                                                            \
        graph->node_id_to_min_rank[0] = graph->node_id_to_max_rank[0] = 0;	                                        \
        for (i = 0; i < graph->node[0].out_edge_n; ++i) { /* set min/max rank for next_id */	                    \
            int out_id = graph->node[0].out_id[i];	                                                                \
            graph->node_id_to_min_rank[out_id] = graph->node_id_to_max_rank[out_id] = 1;	                        \
        }                                                                                                           \
        dp_beg[0] = 0, dp_end[0] = w;                                                                               \
    } else {                                                                                                        \
        dp_beg[0] = 0, dp_end[0] = qlen;	                                                                        \
    }                                                                                                               \
    dp_beg_sn[0] = (dp_beg[0])/pn; dp_end_sn[0] = (dp_end[0])/pn; _end_sn = MIN_OF_TWO(dp_end_sn[0]+1, dp_sn-1);	\
    dp_beg[0] = dp_beg_sn[0] * pn; dp_end[0] = (dp_end_sn[0]+1)*pn-1;                                               \
    dp_h = DP_H;                                                                                                    \
}

#define simd_abpoa_ag_first_row {                                                           \
    /* fill the first row */                                                                \
    if (abpt->bw >= 0) {                                                                    \
        graph->node_id_to_min_rank[0] = graph->node_id_to_max_rank[0] = 0;                  \
        for (i = 0; i < graph->node[0].out_edge_n; ++i) { /* set min/max rank for next_id */\
            int out_id = graph->node[0].out_id[i];                                          \
            graph->node_id_to_min_rank[out_id] = graph->node_id_to_max_rank[out_id] = 1;    \
        }                                                                                   \
        dp_beg[0] = 0, dp_end[0] = w;                                                       \
    } else {                                                                                \
        dp_beg[0] = 0, dp_end[0] = qlen;                                                    \
    }                                                                                       \
    dp_beg_sn[0] = (dp_beg[0])/pn; dp_end_sn[0] = (dp_end[0])/pn;                           \
    dp_beg[0] = dp_beg_sn[0] * pn; dp_end[0] = (dp_end_sn[0]+1)*pn-1;                       \
    dp_h = DP_HE; dp_e = dp_h + dp_sn;                                                      \
    _end_sn = MIN_OF_TWO(dp_end_sn[0]+1, dp_sn-1);                                          \
}

#define simd_abpoa_cg_first_row {                                                                       \
    /* fill the first row */                                                                            \
    if (abpt->bw >= 0) {                                                                                \
        graph->node_id_to_min_rank[0] = graph->node_id_to_max_rank[0] = 0;                              \
        for (i = 0; i < graph->node[0].out_edge_n; ++i) { /* set min/max rank for next_id */            \
            int out_id = graph->node[0].out_id[i];                                                      \
            graph->node_id_to_min_rank[out_id] = graph->node_id_to_max_rank[out_id] = 1;                \
        }                                                                                               \
        dp_beg[0] = 0, dp_end[0] = w;                                                                   \
    } else {                                                                                            \
        dp_beg[0] = 0, dp_end[0] = qlen;                                                                \
    }                                                                                                   \
    dp_beg_sn[0] = (dp_beg[0])/pn; dp_end_sn[0] = (dp_end[0])/pn;                                       \
    dp_beg[0] = dp_beg_sn[0] * pn; dp_end[0] = (dp_end_sn[0]+1)*pn-1;                                   \
    dp_h = DP_H2E; dp_e1 = dp_h + dp_sn; dp_e2 = dp_e1 + dp_sn;                                         \
    _end_sn = MIN_OF_TWO(dp_end_sn[0]+1, dp_sn-1);                                                      \
}

#define simd_abpoa_lg_first_dp(score_t) {                               \
    simd_abpoa_lg_first_row;                                            \
    _dp_h = (score_t*)dp_h;	                                            \
    for (i = dp_end[0]/pn; i <= _end_sn; ++i) {	                        \
        dp_h[i] = SIMD_MIN_INF;	                                        \
    }	                                                                \
    for (i = 0; i <= dp_end[0]; ++i) { /* no SIMD parallelization */	\
        _dp_h[i] = -gap_ext1 * i;	                                    \
    }	                                                                \
}

#define simd_abpoa_ag_first_dp(score_t) {                               \
    simd_abpoa_ag_first_row;                                            \
    _dp_h = (score_t*)dp_h, _dp_e = (score_t*)dp_e;                     \
    for (i = dp_end[0]/pn; i <= _end_sn; ++i) {                         \
        dp_h[i] = SIMD_MIN_INF; dp_e[i] = SIMD_MIN_INF;                 \
    }                                                                   \
    for (i = 1; i <= dp_end[0]; ++i) { /* no SIMD parallelization */    \
        _dp_h[i] = -gap_open1 - gap_ext1 * i;                           \
    } _dp_h[0] = 0; _dp_e[0] = -(gap_oe);                               \
}

#define simd_abpoa_cg_first_dp(score_t) {                                           \
    simd_abpoa_cg_first_row;                                                        \
    _dp_h = (score_t*)dp_h, _dp_e1 = (score_t*)dp_e1, _dp_e2 = (score_t*)dp_e2;     \
    for (i = 0; i <= _end_sn; ++i) {                                                \
        dp_h[i] = SIMD_MIN_INF; dp_e1[i] = SIMD_MIN_INF; dp_e2[i] = SIMD_MIN_INF;   \
    }                                                                               \
    for (i = 1; i <= dp_end[0]; ++i) { /* no SIMD parallelization */                \
        _dp_h[i] = -MIN_OF_TWO(gap_open1+gap_ext1*i, gap_open2+gap_ext2*i);         \
    } _dp_h[0] = 0; _dp_e1[0] = -(gap_oe1); _dp_e2[0] = -(gap_oe2);                 \
}

// mask[pn], suf_min[pn], pre_min[logN]
#define SIMD_SET_F(F, log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN) {  \
    if (set_num == pn) {                                                                                                \
        F = SIMDMax(F, SIMDOri(SIMDShiftLeft(SIMDSub(F, GAP_E1S[0]), SIMDShiftOneN), PRE_MIN[1]));                      \
        if (log_n > 1) {                                                                                                \
            F = SIMDMax(F, SIMDOri(SIMDShiftLeft(SIMDSub(F, GAP_E1S[1]), 2 * SIMDShiftOneN), PRE_MIN[2]));              \
        } if (log_n > 2) {                                                                                              \
            F = SIMDMax(F, SIMDOri(SIMDShiftLeft(SIMDSub(F, GAP_E1S[2]), 4 * SIMDShiftOneN), PRE_MIN[4]));              \
        } if (log_n > 3) {                                                                                              \
            F = SIMDMax(F, SIMDOri(SIMDShiftLeft(SIMDSub(F, GAP_E1S[3]), 8 * SIMDShiftOneN), PRE_MIN[8]));              \
        } if (log_n > 4) {                                                                                              \
            F = SIMDMax(F, SIMDOri(SIMDShiftLeft(SIMDSub(F, GAP_E1S[4]), 16 * SIMDShiftOneN), PRE_MIN[16]));            \
        } if (log_n > 5) {                                                                                              \
            F = SIMDMax(F, SIMDOri(SIMDShiftLeft(SIMDSub(F, GAP_E1S[5]), 32 * SIMDShiftOneN), PRE_MIN[32]));            \
        }                                                                                                               \
    } else { /*suffix MIN_INF*/                                                                                                                                     \
        int cov_bit = set_num;                                                                                                                                      \
        F = SIMDMax(F, SIMDOri(SIMDAndi(SIMDShiftLeft(SIMDSub(F, GAP_E1S[0]), SIMDShiftOneN), PRE_MASK[cov_bit]), SIMDOri(SUF_MIN[cov_bit], PRE_MIN[1])));          \
        if (log_n > 1) {                                                                                                                                            \
            cov_bit += 2;                                                                                                                                           \
            F = SIMDMax(F, SIMDOri(SIMDAndi(SIMDShiftLeft(SIMDSub(F, GAP_E1S[1]), 2 * SIMDShiftOneN), PRE_MASK[cov_bit]), SIMDOri(SUF_MIN[cov_bit], PRE_MIN[2])));  \
        } if (log_n > 2) {                                                                                                                                          \
            cov_bit += 4;                                                                                                                                           \
            F = SIMDMax(F, SIMDOri(SIMDAndi(SIMDShiftLeft(SIMDSub(F, GAP_E1S[2]), 4 * SIMDShiftOneN), PRE_MASK[cov_bit]), SIMDOri(SUF_MIN[cov_bit], PRE_MIN[4])));  \
        } if (log_n > 3) {                                                                                                                                          \
            cov_bit += 8;                                                                                                                                           \
            F = SIMDMax(F, SIMDOri(SIMDAndi(SIMDShiftLeft(SIMDSub(F, GAP_E1S[3]), 8 * SIMDShiftOneN), PRE_MASK[cov_bit]), SIMDOri(SUF_MIN[cov_bit], PRE_MIN[8])));  \
        } if (log_n > 4) {                                                                                                                                          \
            cov_bit += 16;                                                                                                                                          \
            F = SIMDMax(F, SIMDOri(SIMDAndi(SIMDShiftLeft(SIMDSub(F, GAP_E1S[4]), 16 * SIMDShiftOneN), PRE_MASK[cov_bit]), SIMDOri(SUF_MIN[cov_bit], PRE_MIN[16])));\
        } if (log_n > 5) {                                                                                                                                          \
            cov_bit += 32;                                                                                                                                          \
            F = SIMDMax(F, SIMDOri(SIMDAndi(SIMDShiftLeft(SIMDSub(F, GAP_E1S[5]), 32 * SIMDShiftOneN), PRE_MASK[cov_bit]), SIMDOri(SUF_MIN[cov_bit], PRE_MIN[32])));\
        }                                                                                                                                                           \
    }                                                                                                                                                               \
}

#define simd_abpoa_lg_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub) {                                   \
    node_id = abpoa_graph_index_to_node_id(graph, index_i);	                                                    \
    SIMDi *q = qp + graph->node[node_id].base * qp_sn, first, remain;	                                        \
    dp_h = &DP_H[index_i * dp_sn]; _dp_h = (score_t*)dp_h;	                                                    \
    int min_pre_beg_sn, max_pre_end_sn;                                                                         \
    if (abpt->bw < 0) {                                                                                         \
        beg = dp_beg[index_i] = 0, end = dp_end[index_i] = qlen;	                                            \
        beg_sn = dp_beg_sn[index_i] = (dp_beg[index_i])/pn; end_sn = dp_end_sn[index_i] = (dp_end[index_i])/pn; \
        min_pre_beg_sn = 0, max_pre_end_sn = end_sn;                                                            \
    } else {                                                                                                    \
        beg = GET_AD_DP_BEGIN(graph, w, node_id, qlen), end = GET_AD_DP_END(graph, w, node_id, qlen);           \
        beg_sn = beg / pn; min_pre_beg_sn = INT32_MAX, max_pre_end_sn = -1;                                     \
        for (i = 0; i < pre_n[index_i]; ++i) {                                                                  \
            pre_i = pre_index[index_i][i];                                                                      \
            if (min_pre_beg_sn > dp_beg_sn[pre_i]) min_pre_beg_sn = dp_beg_sn[pre_i];                           \
            if (max_pre_end_sn < dp_end_sn[pre_i]) max_pre_end_sn = dp_end_sn[pre_i];                           \
        } if (beg_sn < min_pre_beg_sn) beg_sn = min_pre_beg_sn;                                                 \
        dp_beg_sn[index_i] = beg_sn; beg = dp_beg[index_i] = dp_beg_sn[index_i] * pn;                           \
        end_sn = dp_end_sn[index_i] = end/pn; end = dp_end[index_i] = (dp_end_sn[index_i]+1)*pn-1;              \
    }                                                                                                           \
    /* loop query */	                                                                                                                \
    /* first pre_node */                                                                                                                \
    pre_i = pre_index[index_i][0];	                                                                                                    \
    pre_dp_h = DP_H + pre_i * dp_sn;	                                                                                                \
    pre_end = dp_end[pre_i];	                                                                                                        \
    pre_beg_sn = dp_beg_sn[pre_i];	                                                                                                    \
    /* set M from (pre_i, q_i-1), E from (pre_i, q_i) */	                                                                            \
    if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneN);                \
    else _beg_sn = pre_beg_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneN);	                                    \
    _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);	                                                                        \
    for (i = beg_sn; i < _beg_sn; ++i) dp_h[i] = SIMD_MIN_INF;                                                                          \
    for (i = _end_sn+1; i <= MIN_OF_TWO(end_sn+1, dp_sn-1); ++i) dp_h[i] = SIMD_MIN_INF;                                                \
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */	                                                        \
        remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneN);	                                                                        \
        dp_h[sn_i] = SIMDMax(SIMDAdd(SIMDOri(first, remain), q[sn_i]), SIMDSub(pre_dp_h[sn_i], GAP_E1));	                            \
        first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneN);	                                                        \
    }                                                                                                                                   \
    /* get max m and e */	                                                                                                            \
    for (i = 1; i < pre_n[index_i]; ++i) {	                                                                                            \
        pre_i = pre_index[index_i][i];	                                                                                                \
        pre_dp_h = DP_H + pre_i * dp_sn;	                                                                                            \
        pre_end = dp_end[pre_i];	                                                                                                    \
        pre_beg_sn = dp_beg_sn[pre_i];	                                                                                                \
        /* set M from (pre_i, q_i-1), E from (pre_i, q_i) */	                                                                        \
        if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneN);            \
        else _beg_sn = pre_beg_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneN);	                                \
        _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);	                                                                    \
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */	                                                    \
            remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneN);	                                                                    \
            dp_h[sn_i] = SIMDMax(SIMDAdd(SIMDOri(first, remain), q[sn_i]), SIMDMax(SIMDSub(pre_dp_h[sn_i], GAP_E1), dp_h[sn_i]));	    \
            first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneN);	                                                    \
        } /* now we have max(h,e) stored at dp_h */	                                                                                    \
    }	                                                                                                                                \
    /* new F start */                                                                                                                   \
    first = SIMDOri(SIMDAndi(dp_h[beg_sn], PRE_MASK[0]), SUF_MIN[0]);                                                                   \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) {                                                                                       \
        if (sn_i < min_pre_beg_sn) {                                                                                                    \
            _err_fatal_simple(__func__, "sn_i < min_pre_beg_sn\n");                                                                     \
        } else if (sn_i > max_pre_end_sn) {                                                                                             \
            set_num = sn_i == max_pre_end_sn+1 ? 1 : 0;                                                                                 \
        } else set_num = pn;                                                                                                            \
        dp_h[sn_i] = SIMDMax(dp_h[sn_i], first);                                                                                        \
        SIMD_SET_F(dp_h[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN);          \
        first = SIMDOri(SIMDAndi(SIMDShiftRight(SIMDSub(dp_h[sn_i], GAP_E1), SIMDTotalBytes-SIMDShiftOneN), PRE_MASK[0]), SUF_MIN[0]);  \
    }                                                                                                                                   \
}

#define simd_abpoa_ag_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub, SIMDGetIfGreater, SIMDSetIfGreater) {               \
    node_id = abpoa_graph_index_to_node_id(graph, index_i);                                                                     \
    SIMDi *q = qp + graph->node[node_id].base * qp_sn, first, remain;                                                           \
    dp_h = DP_HE + (index_i<<1) * dp_sn; dp_e = dp_h + dp_sn;                                                                   \
    _dp_h = (score_t*)dp_h, _dp_e = (score_t*)dp_e;                                                                             \
    int min_pre_beg_sn, max_pre_end_sn;                                                                                         \
    if (abpt->bw < 0) {                                                                                                         \
        beg = dp_beg[index_i] = 0, end = dp_end[index_i] = qlen;                                                                \
        beg_sn = dp_beg_sn[index_i] = (dp_beg[index_i])/pn; end_sn = dp_end_sn[index_i] = (dp_end[index_i])/pn;                 \
        min_pre_beg_sn = 0, max_pre_end_sn = end_sn;                                                                            \
    } else {                                                                                                                    \
        beg = GET_AD_DP_BEGIN(graph, w, node_id, qlen), end = GET_AD_DP_END(graph, w, node_id, qlen);                           \
        beg_sn = beg / pn; min_pre_beg_sn = INT32_MAX, max_pre_end_sn = -1;                                                     \
        for (i = 0; i < pre_n[index_i]; ++i) {                                                                                  \
            pre_i = pre_index[index_i][i];                                                                                      \
            if (min_pre_beg_sn > dp_beg_sn[pre_i]) min_pre_beg_sn = dp_beg_sn[pre_i];                                           \
            if (max_pre_end_sn < dp_end_sn[pre_i]) max_pre_end_sn = dp_end_sn[pre_i];                                           \
        } if (beg_sn < min_pre_beg_sn) beg_sn = min_pre_beg_sn;                                                                 \
        dp_beg_sn[index_i] = beg_sn; beg = dp_beg[index_i] = dp_beg_sn[index_i] * pn;                                           \
        end_sn = dp_end_sn[index_i] = end/pn; end = dp_end[index_i] = (dp_end_sn[index_i]+1)*pn-1;                              \
    }                                                                                                                           \
    /* loop query */                                                                                                            \
    /* first pre_node */                                                                                                        \
    pre_i = pre_index[index_i][0];                                                                                              \
    pre_dp_h = DP_HE + (pre_i << 1) * dp_sn; pre_dp_e = pre_dp_h + dp_sn;                                                       \
    pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];                                      \
    /* set M from (pre_i, q_i-1) */                                                                                             \
    if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneN);        \
    else _beg_sn = pre_beg_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneN);	                            \
    _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);                                                                    \
    for (i = beg_sn; i < _beg_sn; ++i) dp_h[i] = SIMD_MIN_INF;                                                                  \
    for (i = _end_sn+1; i <= MIN_OF_TWO(end_sn+1, dp_sn-1); ++i) dp_h[i] = SIMD_MIN_INF;                                        \
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                                  \
        remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneN);                                                                  \
        dp_h[sn_i] = SIMDOri(first, remain);                                                                                    \
        first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneN);                                                   \
    }                                                                                                                           \
    /* set E from (pre_i, q_i) */                                                                                               \
    _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);                                                                                   \
    for (i = beg_sn; i < _beg_sn; ++i) dp_e[i] = SIMD_MIN_INF;                                                                  \
    for (i = _end_sn+1; i <= end_sn; ++i) dp_e[i] = SIMD_MIN_INF;                                                               \
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i)   /* SIMD parallelization */                                                  \
        dp_e[sn_i] = pre_dp_e[sn_i];                                                                                            \
    /* get max m and e */                                                                                                       \
    for (i = 1; i < pre_n[index_i]; ++i) {                                                                                      \
        pre_i = pre_index[index_i][i];                                                                                          \
        pre_dp_h = DP_HE + (pre_i << 1) * dp_sn; pre_dp_e = pre_dp_h + dp_sn;                                                   \
        pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];                                  \
        /* set M from (pre_i, q_i-1) */                                                                                         \
        if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneN);    \
        else _beg_sn = pre_beg_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneN);	                        \
        _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);                                                                \
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                              \
            remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneN);                                                              \
            dp_h[sn_i] = SIMDMax(SIMDOri(first, remain), dp_h[sn_i]);                                                           \
            first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneN);                                               \
        }                                                                                                                       \
        /* set E from (pre_i, q_i) */                                                                                           \
        _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);                                                                               \
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i)   /* SIMD parallelization */                                              \
            dp_e[sn_i] = SIMDMax(pre_dp_e[sn_i], dp_e[sn_i]);                                                                   \
    }                                                                                                                           \
    /* compare M, E, and F */                                                                                                   \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */                                                    \
        dp_h[sn_i] = SIMDMax(SIMDAdd(dp_h[sn_i], q[sn_i]), dp_e[sn_i]);                                                         \
    }                                                                                                                           \
    /* new F start */                                                                                                           \
    first = SIMDShiftRight(SIMDShiftLeft(dp_h[beg_sn], SIMDTotalBytes-SIMDShiftOneN), SIMDTotalBytes-SIMDShiftOneN);            \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) {                                                                               \
        if (sn_i < min_pre_beg_sn) {                                                                                            \
            _err_fatal_simple(__func__, "sn_i < min_pre_beg_sn\n");                                                             \
        } else if (sn_i > max_pre_end_sn) {                                                                                     \
            set_num = sn_i == max_pre_end_sn+1 ? 2 : 1;                                                                         \
        } else set_num = pn;                                                                                                    \
        /* F = (H << 1 | x) - OE */                                                                                             \
        dp_f[sn_i] = SIMDSub(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneN), first), GAP_OE1);                                \
        /* F = max{F, (F-e)<<1}, F = max{F, (F-2e)<<2} ... */                                                                   \
        SIMD_SET_F(dp_f[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN);  \
        /* x = max{H, F+o} */                                                                                                   \
        first = SIMDShiftRight(SIMDMax(dp_h[sn_i], SIMDAdd(dp_f[sn_i], GAP_O1)), SIMDTotalBytes-SIMDShiftOneN);                 \
        /* H = max{H, F} */                                                                                                     \
        dp_h[sn_i] = SIMDMax(dp_h[sn_i], dp_f[sn_i]);                                                                           \
    }                                                                                                                           \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */                                                    \
        /* e for next cell */                                                                                                   \
        dp_e[sn_i] = SIMDMax(SIMDSub(dp_e[sn_i], GAP_E1), SIMDSub(dp_h[sn_i], GAP_OE1));                                        \
    }                                                                                                                           \
}

#define simd_abpoa_cg_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub, SIMDGetIfGreater, SIMDSetIfGreater) {               \
    node_id = abpoa_graph_index_to_node_id(graph, index_i);                                                                     \
    SIMDi *q = qp + graph->node[node_id].base * qp_sn, first, remain;                                                           \
    dp_h = DP_H2E + (index_i*3) * dp_sn; dp_e1 = dp_h + dp_sn; dp_e2 = dp_e1 + dp_sn;                                           \
    _dp_h = (score_t*)dp_h, _dp_e1 = (score_t*)dp_e1, _dp_e2 = (score_t*)dp_e2;                                                 \
    int min_pre_beg_sn, max_pre_end_sn;                                                                                         \
    if (abpt->bw < 0) {                                                                                                         \
        beg = dp_beg[index_i] = 0, end = dp_end[index_i] = qlen;                                                                \
        beg_sn = dp_beg_sn[index_i] = beg/pn; end_sn = dp_end_sn[index_i] = end/pn;                                             \
        min_pre_beg_sn = 0, max_pre_end_sn = end_sn;                                                                            \
    } else {                                                                                                                    \
        beg = GET_AD_DP_BEGIN(graph, w, node_id, qlen), end = GET_AD_DP_END(graph, w, node_id, qlen);                           \
        beg_sn = beg / pn; min_pre_beg_sn = INT32_MAX, max_pre_end_sn = -1;                                                     \
        for (i = 0; i < pre_n[index_i]; ++i) {                                                                                  \
            pre_i = pre_index[index_i][i];                                                                                      \
            if (min_pre_beg_sn > dp_beg_sn[pre_i]) min_pre_beg_sn = dp_beg_sn[pre_i];                                           \
            if (max_pre_end_sn < dp_end_sn[pre_i]) max_pre_end_sn = dp_end_sn[pre_i];                                           \
        } if (beg_sn < min_pre_beg_sn) beg_sn = min_pre_beg_sn;                                                                 \
        dp_beg_sn[index_i] = beg_sn; beg = dp_beg[index_i] = dp_beg_sn[index_i] * pn;                                           \
        end_sn = dp_end_sn[index_i] = end/pn; end = dp_end[index_i] = (dp_end_sn[index_i]+1)*pn-1;                              \
    }                                                                                                                           \
 /* fprintf(stderr, "index_i: %d, beg_sn: %d, end_sn: %d\n", index_i, beg_sn, end_sn); */                                       \
    /* tot_dp_sn += (end_sn - beg_sn + 1); */                                                                                   \
    /* loop query */                                                                                                            \
    /* first pre_node */                                                                                                        \
    pre_i = pre_index[index_i][0];                                                                                              \
    pre_dp_h = DP_H2E + (pre_i * 3) * dp_sn; pre_dp_e1 = pre_dp_h + dp_sn; pre_dp_e2 = pre_dp_e1 + dp_sn;                       \
    pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];                                      \
    /* set M from (pre_i, q_i-1) */                                                                                             \
    if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneN);        \
    else _beg_sn = pre_beg_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneN);                              \
    _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);                                                                    \
    for (i = beg_sn; i < _beg_sn; ++i) dp_h[i] = SIMD_MIN_INF;                                                                  \
    for (i = _end_sn+1; i <= MIN_OF_TWO(end_sn+1, dp_sn-1); ++i) dp_h[i] = SIMD_MIN_INF;                                        \
 /* fprintf(stderr, "1 index_i: %d, beg_sn: %d, end_sn: %d\n", index_i, _beg_sn, _end_sn); */                                   \
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                                  \
        remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneN);                                                                  \
        dp_h[sn_i] = SIMDOri(first, remain);                                                                                    \
        first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneN);                                                   \
    }                                                                                                                           \
    /* set E from (pre_i, q_i) */                                                                                               \
    _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);                                                                                   \
    for (i = beg_sn; i < _beg_sn; ++i) dp_e1[i] = SIMD_MIN_INF, dp_e2[i] = SIMD_MIN_INF;                                        \
    for (i = _end_sn+1; i <= end_sn; ++i) dp_e1[i] = SIMD_MIN_INF, dp_e2[i] = SIMD_MIN_INF;                                     \
 /* fprintf(stderr, "2 index_i: %d, beg_sn: %d, end_sn: %d\n", index_i, _beg_sn, _end_sn); */                                   \
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                                  \
        dp_e1[sn_i] = pre_dp_e1[sn_i];                                                                                          \
        dp_e2[sn_i] = pre_dp_e2[sn_i];                                                                                          \
    }                                                                                                                           \
    /* get max m and e */                                                                                                       \
    for (i = 1; i < pre_n[index_i]; ++i) {                                                                                      \
        pre_i = pre_index[index_i][i];                                                                                          \
        pre_dp_h = DP_H2E + (pre_i * 3) * dp_sn; pre_dp_e1 = pre_dp_h + dp_sn; pre_dp_e2 = pre_dp_e1 + dp_sn;                   \
        pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];                                  \
        /* set M from (pre_i, q_i-1) */                                                                                         \
        if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneN);    \
        else _beg_sn = pre_beg_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneN);                          \
        _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);                                                                \
     /* fprintf(stderr, "3 index_i: %d, beg_sn: %d, end_sn: %d\n", index_i, _beg_sn, _end_sn); */                               \
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                              \
            remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneN);                                                              \
            dp_h[sn_i] = SIMDMax(SIMDOri(first, remain), dp_h[sn_i]);                                                           \
            first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneN);                                               \
        }                                                                                                                       \
        /* set E from (pre_i, q_i) */                                                                                           \
        _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);                                                                               \
     /* fprintf(stderr, "4 index_i: %d, beg_sn: %d, end_sn: %d\n", index_i, _beg_sn, _end_sn); */                               \
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                              \
            dp_e1[sn_i] = SIMDMax(pre_dp_e1[sn_i], dp_e1[sn_i]);                                                                \
            dp_e2[sn_i] = SIMDMax(pre_dp_e2[sn_i], dp_e2[sn_i]);                                                                \
        }                                                                                                                       \
    }                                                                                                                           \
    /* compare M, E, and F */                                                                                                   \
 /* fprintf(stderr, "5 index_i: %d, beg_sn: %d, end_sn: %d\n", index_i, _beg_sn, _end_sn); */                                   \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */                                                    \
        dp_h[sn_i] = SIMDMax(SIMDAdd(dp_h[sn_i], q[sn_i]), dp_e1[sn_i]);                                                        \
        dp_h[sn_i] = SIMDMax(dp_h[sn_i], dp_e2[sn_i]);                                                                          \
    }                                                                                                                           \
    /* new F start */                                                                                                           \
    first = SIMDShiftRight(SIMDShiftLeft(dp_h[beg_sn], SIMDTotalBytes-SIMDShiftOneN), SIMDTotalBytes-SIMDShiftOneN);            \
    SIMDi first2 = first;                                                                                                       \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) {                                                                               \
        if (sn_i < min_pre_beg_sn) {                                                                                            \
            _err_fatal_simple(__func__, "sn_i < min_pre_beg_sn\n");                                                             \
        } else if (sn_i > max_pre_end_sn) {                                                                                     \
            set_num = sn_i == max_pre_end_sn+1 ? 2 : 1;                                                                         \
        } else set_num = pn;                                                                                                    \
        /* F = (H << 1 | x) - OE */                                                                                             \
        dp_f[sn_i] = SIMDSub(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneN), first), GAP_OE1);                                \
        dp_f2[sn_i] = SIMDSub(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneN), first2), GAP_OE2);                              \
        /* F = max{F, (F-e)<<1}, F = max{F, (F-2e)<<2} ... */                                                                   \
        SIMD_SET_F(dp_f[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN);  \
        SIMD_SET_F(dp_f2[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E2S, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN); \
        /* x = max{H, F+o} */                                                                                                   \
        first = SIMDShiftRight(SIMDMax(dp_h[sn_i], SIMDAdd(dp_f[sn_i], GAP_O1)), SIMDTotalBytes-SIMDShiftOneN);                 \
        first2 = SIMDShiftRight(SIMDMax(dp_h[sn_i], SIMDAdd(dp_f2[sn_i], GAP_O2)), SIMDTotalBytes-SIMDShiftOneN);               \
        /* H = max{H, F}    */                                                                                                  \
        dp_h[sn_i] = SIMDMax(dp_h[sn_i], dp_f[sn_i]);                                                                           \
        dp_h[sn_i] = SIMDMax(dp_h[sn_i], dp_f2[sn_i]);                                                                          \
    }                                                                                                                           \
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */                                                    \
        /* e for next cell */                                                                                                   \
        dp_e1[sn_i] = SIMDMax(SIMDSub(dp_e1[sn_i], GAP_E1), SIMDSub(dp_h[sn_i], GAP_OE1));                                      \
        dp_e2[sn_i] = SIMDMax(SIMDSub(dp_e2[sn_i], GAP_E2), SIMDSub(dp_h[sn_i], GAP_OE2));                                      \
    }                                                                                                                           \
}

#define set_max_score_global(best_score, best_i, best_j, score, i, j) { \
    if (score > best_score) {                                           \
        best_score = score; best_i = i; best_j = j;                     \
    }                                                                   \
}

#define set_max_score_extend(best_score, best_i, best_j, best_id, score, i, j) {                            \
    if (score > best_score) {                                                                               \
        best_score = score; best_i = i; best_j = j; best_id = node_id;                                      \
    } else if (abpt->zdrop > 0) {                                                                           \
        int delta_index = graph->node_id_to_max_remain[best_id] - graph->node_id_to_max_remain[node_id];    \
        if (best_score - score > abpt->zdrop + gap_ext1 * abs(delta_index-(j-best_j)))                      \
            break;                                                                                          \
    }                                                                                                       \
}

#define simd_abpoa_global_get_max(score_t, DP_H_HE, dp_sn) {	                        \
    int in_id, in_index;	                                                            \
    for (i = 0; i < graph->node[ABPOA_SINK_NODE_ID].in_edge_n; ++i) {	                \
        in_id = graph->node[ABPOA_SINK_NODE_ID].in_id[i];	                            \
        in_index = abpoa_graph_node_id_to_index(graph, in_id);	                        \
        dp_h = DP_H_HE + in_index * dp_sn;	                                            \
        _dp_h = (score_t*)dp_h;	                                                        \
        set_max_score_global(best_score, best_i, best_j, _dp_h[qlen], in_index, qlen);  \
    }	                                                                                \
}

#define simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater) {\
    /* select max dp_h */                                                   \
    max = inf_min, max_i = -1;                                              \
    SIMDi a = dp_h[end_sn], b = qi[end_sn];                                 \
    if (end_sn == qlen / pn) SIMDSetIfGreater(a, zero, b, SIMD_MIN_INF, a); \
    for (i = beg_sn; i < end_sn; ++i) {                                     \
        SIMDGetIfGreater(b, a, dp_h[i], a, qi[i], b);                       \
    }                                                                       \
    _dp_h = (score_t*)&a, _qi = (score_t*)&b;                               \
    for (i = 0; i < pn; ++i) {                                              \
        if (_dp_h[i] > max) {                                               \
            max = _dp_h[i]; max_i = _qi[i];                                 \
        }                                                                   \
    }                                                                       \
}

#define simd_abpoa_ada_max_i   {                                                                                \
    /* set min/max_rank for next nodes */                                                                       \
    int out_i = max_i + 1;                                                                                      \
    for (i = 0; i < graph->node[node_id].out_edge_n; ++i) {                                                     \
        int out_node_id = graph->node[node_id].out_id[i];                                                       \
        if (out_i > graph->node_id_to_max_rank[out_node_id]) graph->node_id_to_max_rank[out_node_id] = out_i;   \
        if (out_i < graph->node_id_to_min_rank[out_node_id]) graph->node_id_to_min_rank[out_node_id] = out_i;   \
    }                                                                                                           \
}

#define simd_abpoa_lg_get_cigar(score_t) {	                                    \
    if (abpt->ret_cigar) {	                                                    \
        simd_abpoa_lg_backtrack(score_t, DP_H, dp_sn, (score_t)abpt->m,         \
            (score_t)abpt->mat, gap_ext1, pre_index, pre_n, 0, 0,               \
            best_i, best_j, qlen, graph, query, res);	                        \
    }	                                                                        \
}


#define simd_abpoa_ag_get_cigar(score_t) {                                      \
    if (abpt->ret_cigar) {                                                      \
        simd_abpoa_ag_backtrack(score_t, DP_HE, dp_sn, (score_t)abpt->m,        \
            (score_t)abpt->mat, gap_ext1, pre_index, pre_n, 0, 0,               \
            best_i, best_j, qlen, graph, query, res);                           \
    }                                                                           \
} 

#define simd_abpoa_cg_get_cigar(score_t) {                                      \
    if (abpt->ret_cigar) {                                                      \
        simd_abpoa_cg_backtrack(score_t, DP_H2E, dp_sn, (score_t)abpt->m,       \
            (score_t)abpt->mat, gap_ext1, gap_ext2, gap_oe1, pre_index, pre_n,  \
            0, 0, best_i, best_j, qlen, graph, query, res);                     \
    }                                                                           \
}

// linear gap penalty: gap_open1 == 0
#define simd_abpoa_lg_global_align_sequence_to_graph_core(score_t, ab, query, qlen, abpt, res, sp,          \
        SIMDSetOne, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN, SIMDSetIfGreater, SIMDGetIfGreater, b_score) {\
    simd_abpoa_lg_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd);                                     \
    simd_abpoa_lg_first_dp(score_t);                                                                        \
    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {                                                \
        simd_abpoa_lg_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub);                                \
        if (abpt->bw >= 0) {                                                                                \
            simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater);                             \
            simd_abpoa_ada_max_i;                                                                           \
        }                                                                                                   \
    }                                                                                                       \
    simd_abpoa_global_get_max(score_t, DP_H, dp_sn);                                                        \
    /*simd_abpoa_print_lg_matrix(score_t);*/                                                                \
    /*printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score);*/                                 \
    simd_abpoa_lg_get_cigar(score_t);                                                                       \
    simd_abpoa_free_var; free(GAP_E1S);                                                                     \
    b_score = best_score;                                                                                   \
} 

// affine gap penalty: gap_open1 > 0
#define simd_abpoa_ag_global_align_sequence_to_graph_core(score_t, ab, query, qlen, abpt, res, sp,                  \
    SIMDSetOne, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN, SIMDSetIfGreater, SIMDGetIfGreater, b_score) {            \
    simd_abpoa_ag_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd);                                             \
    simd_abpoa_ag_first_dp(score_t);                                                                                \
    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {                                                        \
        simd_abpoa_ag_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub, SIMDGetIfGreater, SIMDSetIfGreater);    \
        if (abpt->bw >= 0) {                                                                                        \
            simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater);                                     \
            simd_abpoa_ada_max_i;                                                                                   \
        }                                                                                                           \
    }                                                                                                               \
    simd_abpoa_global_get_max(score_t, DP_HE, 2*dp_sn);                                                             \
    /* simd_abpoa_print_ag_matrix(score_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score); */  \
    simd_abpoa_ag_get_cigar(score_t);                                                                               \
    simd_abpoa_free_var; free(GAP_E1S);                                                                             \
    b_score = best_score;                                                                                           \
}

// convex gap penalty: gap_open1 > 0 && gap_open2 > 0
#define simd_abpoa_cg_global_align_sequence_to_graph_core(score_t, ab, query, qlen, abpt, res, sp,              \
    SIMDSetOne, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN, SIMDSetIfGreater, SIMDGetIfGreater, b_score) {        \
    simd_abpoa_cg_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd);                                         \
    simd_abpoa_cg_first_dp(score_t);                                                                            \
    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {                                                    \
        simd_abpoa_cg_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub, SIMDGetIfGreater, SIMDSetIfGreater);\
        if (abpt->bw >= 0) {                                                                                    \
            simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater);                                 \
            simd_abpoa_ada_max_i;                                                                               \
        }                                                                                                       \
    }                                                                                                           \
 /* printf("dp_sn: %d\n", tot_dp_sn); */                                                                        \
    simd_abpoa_global_get_max(score_t, DP_H2E, 3*dp_sn);                                                        \
 /* simd_abpoa_print_cg_matrix(score_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score); */ \
    simd_abpoa_cg_get_cigar(score_t);                                                                           \
    simd_abpoa_free_var; free(GAP_E1S); free(GAP_E2S);                                                          \
    b_score = best_score;                                                                                       \
}

// TODO end_bonus for extension
#define simd_abpoa_lg_extend_align_sequence_to_graph_core(score_t, ab, query, qlen, abpt, res, sp,                  \
    SIMDSetOne, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN, SIMDSetIfGreater, SIMDGetIfGreater, b_score) {            \
    simd_abpoa_lg_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd);                                             \
    int best_id = 0;                                                                                                \
    simd_abpoa_lg_first_dp(score_t);                                                                                \
    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {                                                        \
        simd_abpoa_lg_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub);                                        \
        simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater);                                         \
        set_max_score_extend(best_score, best_i, best_j, best_id, max, index_i, max_i);                             \
        if (abpt->bw >= 0) {                                                                                        \
            simd_abpoa_ada_max_i;                                                                                   \
        }                                                                                                           \
    }                                                                                                               \
    /* simd_abpoa_print_lg_matrix(score_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score); */  \
    simd_abpoa_lg_get_cigar(score_t);                                                                               \
    simd_abpoa_free_var; free(GAP_E1S);                                                                             \
    b_score = best_score;                                                                                           \
}
 
#define simd_abpoa_ag_extend_align_sequence_to_graph_core(score_t, ab, query, qlen, abpt, res, sp,                  \
        SIMDSetOne, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN, SIMDSetIfGreater, SIMDGetIfGreater, b_score) {        \
    simd_abpoa_ag_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd);                                             \
    int best_id = 0;                                                                                                \
    simd_abpoa_ag_first_dp(score_t);                                                                                \
    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {                                                        \
        simd_abpoa_ag_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub, SIMDGetIfGreater, SIMDSetIfGreater);    \
        simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater);                                         \
        set_max_score_extend(best_score, best_i, best_j, best_id, max, index_i, max_i);                             \
        /* set adaptive band */                                                                                     \
        if (abpt->bw >= 0) {                                                                                        \
            simd_abpoa_ada_max_i;                                                                                   \
        }                                                                                                           \
    }                                                                                                               \
    /*simd_abpoa_print_ag_matrix(score_t);  */                                                                      \
 /* printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score); */                                          \
    simd_abpoa_ag_get_cigar(score_t);                                                                               \
    simd_abpoa_free_var; free(GAP_E1S);                                                                             \
    b_score = best_score;                                                                                           \
}

#define simd_abpoa_cg_extend_align_sequence_to_graph_core(score_t, ab, query, qlen, abpt, res, sp,                  \
        SIMDSetOne, SIMDMax, SIMDAdd, SIMDSub, SIMDShiftOneN, SIMDSetIfGreater, SIMDGetIfGreater, b_score) {        \
    simd_abpoa_cg_var(score_t, sp, SIMDSetOne, SIMDShiftOneN, SIMDAdd);                                             \
    simd_abpoa_cg_first_dp(score_t);                                                                                \
    int best_id = 0;                                                                                                \
    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {                                                        \
        simd_abpoa_cg_dp(score_t, SIMDShiftOneN, SIMDMax, SIMDAdd, SIMDSub, SIMDGetIfGreater, SIMDSetIfGreater);    \
        simd_abpoa_max_in_row(score_t, SIMDSetIfGreater, SIMDGetIfGreater);                                         \
        set_max_score_extend(best_score, best_i, best_j, best_id, max, index_i, max_i);                             \
        /* set adaptive band */                                                                                     \
        if (abpt->bw >= 0) {                                                                                        \
            simd_abpoa_ada_max_i;                                                                                   \
        }                                                                                                           \
    }                                                                                                               \
 /* simd_abpoa_print_cg_matrix(score_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score); */     \
    simd_abpoa_cg_get_cigar(score_t);                                                                               \
    simd_abpoa_free_var; free(GAP_E1S); free(GAP_E2S);                                                              \
    b_score = best_score;                                                                                           \
}


abpoa_simd_matrix_t *abpoa_init_simd_matrix(void) {
    abpoa_simd_matrix_t *abm = (abpoa_simd_matrix_t*)_err_malloc(sizeof(abpoa_simd_matrix_t));
    abm->s_msize = 0; abm->s_mem = NULL; abm->rang_m = 0; 
    abm->dp_beg = NULL; abm->dp_end = NULL; abm->dp_beg_sn = NULL; abm->dp_end_sn = NULL;
    return abm;
}

void abpoa_free_simd_matrix(abpoa_simd_matrix_t *abm) {
    if (abm->s_mem) free(abm->s_mem);
    if (abm->dp_beg) {
        free(abm->dp_beg); free(abm->dp_end); free(abm->dp_beg_sn); free(abm->dp_end_sn);
    } free(abm);
}

// realloc memory everytime the graph is updated (nodes are updated already)
// * index_to_node_id/node_id_to_index/node_id_to_max/min_rank/remain
// * qp, DP_HE/H (if ag/lg), dp_f, qi (if ada/extend)
// * dp_beg/end, dp_beg/end_sn if band
// * pre_n, pre_index
int simd_abpoa_realloc(abpoa_t *ab, int qlen, abpoa_para_t *abpt, SIMD_para_t sp) {
    uint64_t pn = sp.num_of_value, size = sp.size, sn = (qlen + sp.num_of_value) / pn, node_n = ab->abg->node_n;
    uint64_t s_msize = sn * abpt->m * size; // qp

    if (abpt->gap_mode == ABPOA_LINEAR_GAP) s_msize += (sn * node_n * size); // DP_H, linear
    else if (abpt->gap_mode == ABPOA_AFFINE_GAP) s_msize += (sn * ((node_n << 1) + 1) * size); // DP_HE + dp_f, affine
    else s_msize += (sn * ((node_n * 3) + 2) * size); // DP_H2E + dp_2f, convex

    if (abpt->bw >= 0 || abpt->align_mode == ABPOA_EXTEND_MODE) // qi
        s_msize += sn * size;

    if (s_msize > UINT32_MAX) {
        err_func_format_printf(__func__, "Warning: Graph is too large or query is too long.\n");
        return 1;
    }
    if (s_msize > ab->abm->s_msize) {
        if (ab->abm->s_mem) free(ab->abm->s_mem);
        ab->abm->s_msize = MAX_OF_TWO(ab->abm->s_msize << 1, s_msize);
        ab->abm->s_mem = (SIMDi*)SIMDMalloc(ab->abm->s_msize, size);
    }

    if ((int)node_n > ab->abm->rang_m) {
        ab->abm->rang_m = MAX_OF_TWO(ab->abm->rang_m << 1, (int)node_n);
        ab->abm->dp_beg = (int*)_err_realloc(ab->abm->dp_beg, ab->abm->rang_m * sizeof(int));
        ab->abm->dp_end = (int*)_err_realloc(ab->abm->dp_end, ab->abm->rang_m * sizeof(int));
        ab->abm->dp_beg_sn = (int*)_err_realloc(ab->abm->dp_beg_sn, ab->abm->rang_m * sizeof(int));
        ab->abm->dp_end_sn = (int*)_err_realloc(ab->abm->dp_end_sn, ab->abm->rang_m * sizeof(int));
    }
    return 0;
}

// -e7 -w XXX
int abpoa_lg_global_align_sequence_to_graph_core(abpoa_t *ab, int qlen, uint8_t *query, abpoa_para_t *abpt, SIMD_para_t sp, abpoa_res_t *res) {
    //simd_abpoa_lg_var(int16_t, sp, SIMDSetOnei16);                                                                         
    //simd_abpoa_var(int16_t, sp, SIMDSetOnei16);        
    // int tot_dp_sn = 0;  
    abpoa_graph_t *graph = ab->abg; abpoa_simd_matrix_t *abm = ab->abm;                             
    int matrix_row_n = graph->node_n, matrix_col_n = qlen + 1;                                      
    int **pre_index, *pre_n, pre_i;                                                                 
    int i, j, k, *dp_beg, *dp_beg_sn, *dp_end, *dp_end_sn, node_id, index_i;          
    int beg, end, beg_sn, end_sn, _beg_sn, _end_sn, pre_beg_sn, pre_end, sn_i;
    int pn, log_n, size, qp_sn, dp_sn; /* pn: value per SIMDi, qp_sn/dp_sn/d_sn: segmented length*/              
    SIMDi *dp_h, *pre_dp_h, *qp, *qi=NULL;                                                   
    int16_t *_dp_h=NULL, *_qi, best_score = sp.inf_min, inf_min = sp.inf_min;               
    int *mat = abpt->mat, best_i = 0, best_j = 0, max, max_i; int16_t gap_ext1 = abpt->gap_ext1;    
    int w = abpt->bw < 0 ? qlen : abpt->bw; /* when w < 0, do whole global */                       
    SIMDi zero = SIMDSetZeroi(), SIMD_MIN_INF = SIMDSetOnei16(inf_min);                                     
    pn = sp.num_of_value; qp_sn = dp_sn = (matrix_col_n + pn - 1) / pn, log_n = sp.log_num, size = sp.size; 
    qp = abm->s_mem;                                                                                

    int set_num; SIMDi *PRE_MASK, *SUF_MIN, *PRE_MIN;   
    PRE_MASK = (SIMDi*)SIMDMalloc((pn+1) * size, size);     
    SUF_MIN = (SIMDi*)SIMDMalloc((pn+1) * size, size);      
    PRE_MIN = (SIMDi*)SIMDMalloc(pn * size, size);          
    for (i = 0; i < pn; ++i) {    
        int16_t *pre_mask = (int16_t*)(PRE_MASK+i);  
        for (j = 0; j <= i; ++j) pre_mask[j] = -1;   
        for (j = i+1; j < pn; ++j) pre_mask[j] = 0;  
    } PRE_MASK[pn] = PRE_MASK[pn-1];  
    SUF_MIN[0] = SIMDShiftLeft(SIMD_MIN_INF, SIMDShiftOneNi16);     
    for (i = 1; i < pn; ++i) SUF_MIN[i] = SIMDShiftLeft(SUF_MIN[i-1], SIMDShiftOneNi16); SUF_MIN[pn] = SUF_MIN[pn-1];   
    for (i = 1; i < pn; ++i) {  
        int16_t *pre_min = (int16_t*)(PRE_MIN + i);   
        for (j = 0; j < i; ++j) pre_min[j] = inf_min;    
        for (j = i; j < pn; ++j) pre_min[j] = 0; 
    }   

    // simd_abpoa_lg_only_var(int16_t, SIMDSetOnei16);    
    SIMDi *DP_H = qp + qp_sn * abpt->m; SIMDi *dp_f = DP_H;                         
    SIMDi GAP_E1 = SIMDSetOnei16(gap_ext1); 
    SIMDi *GAP_E1S =  (SIMDi*)SIMDMalloc(log_n * size, size);      
    GAP_E1S[0] = GAP_E1;  
    for (i = 1; i < log_n; ++i) {   
        GAP_E1S[i] = SIMDAddi16(GAP_E1S[i-1], GAP_E1S[i-1]);   
    }   

    // simd_abpoa_init_var(int16_t, sp);
    /* generate the query profile */                                                                                        
    for (i = 0; i < qp_sn * abpt->m; ++i) qp[i] = SIMD_MIN_INF;                                                                  
    for (k = 0; k < abpt->m; ++k) { /* SIMD parallelization */                                                              
        int *p = &mat[k * abpt->m];                                                                                         
        int16_t *_qp = (int16_t*)(qp + k * qp_sn); _qp[0] = 0;                                                              
        for (j = 0; j < qlen; ++j) _qp[j+1] = (int16_t)p[query[j]];                                                         
        for (j = qlen+1; j < qp_sn * pn; ++j) _qp[j] = 0;                                                                   
    }                                                                                                                       
    if (abpt->bw >= 0 || abpt->align_mode == ABPOA_EXTEND_MODE) { /* query index */ 
        qi = dp_f + dp_sn; _qi = (int16_t*)qi;                                                                              
        for (i = 0; i <= qlen; ++i) _qi[i] = i;                                                                             
        for (i = qlen+1; i < (qlen/pn+1) * pn; ++i) _qi[i] = -1;                                                            
    }                                                                                                                       
    /* for backtrack */                                                                                                     
    dp_beg = abm->dp_beg, dp_end = abm->dp_end, dp_beg_sn = abm->dp_beg_sn, dp_end_sn = abm->dp_end_sn;                     
    if (abpt->bw >= 0) {                                                                                                    
        for (i = 0; i < graph->node_n; ++i) {                                                                               
            // node_id = graph->index_to_node_id[i];                                                                           
            graph->node_id_to_min_rank[i] = graph->node_n;                                                            
            graph->node_id_to_max_rank[i] = 0;                                                                        
        }                                                                                                                   
    }                                                                                                                       
    /* index of pre-node */                                                                                                 
    pre_index = (int**)_err_malloc(graph->node_n * sizeof(int*));                                                           
    pre_n = (int*)_err_malloc(graph->node_n * sizeof(int*));                                                                
    for (i = 0; i < graph->node_n; ++i) {                                                                                   
        node_id = abpoa_graph_index_to_node_id(graph, i); /* i: node index */                                               
        pre_n[i] = graph->node[node_id].in_edge_n;                                                                          
        pre_index[i] = (int*)_err_malloc(pre_n[i] * sizeof(int));                                                           
        for (j = 0; j < pre_n[i]; ++j) {                                                                                    
            pre_index[i][j] = abpoa_graph_node_id_to_index(graph, graph->node[node_id].in_id[j]);                           
        }                                                                                                                   
    }                                                                                                                       
    //simd_abpoa_cg_first_dp(int16_t);                                                                                    
    //simd_abpoa_cg_first_row;                                                    
    /* fill the first row */                                                                    
    if (abpt->bw >= 0) {                                                                                            
        graph->node_id_to_min_rank[0] = graph->node_id_to_max_rank[0] = 0;	                                        
        for (i = 0; i < graph->node[0].out_edge_n; ++i) { /* set min/max rank for next_id */	                    
            int out_id = graph->node[0].out_id[i];	                                                                
            graph->node_id_to_min_rank[out_id] = graph->node_id_to_max_rank[out_id] = 1;	                        
        }                                                                                                           
        dp_beg[0] = GET_AD_DP_BEGIN(graph, w, 0, qlen), dp_end[0] = GET_AD_DP_END(graph, w, 0, qlen);	            
    }else {                                                                                                         
        dp_beg[0] = 0, dp_end[0] = qlen;	                                                                        
    }                                                                                                               
    dp_beg_sn[0] = (dp_beg[0])/pn; dp_end_sn[0] = (dp_end[0])/pn; _end_sn = MIN_OF_TWO(dp_end_sn[0]+1, dp_sn-1);	
    dp_beg[0] = dp_beg_sn[0] * pn; dp_end[0] = (dp_end_sn[0]+1)*pn-1;                                   
    dp_h = DP_H;

    _dp_h = (int16_t*)dp_h;	                                            
    for (i = 0; i <= _end_sn; ++i) {	                                
        dp_h[i] = SIMD_MIN_INF;	                                            
    }	                                                                
    for (i = 0; i <= dp_end[0]; ++i) { /* no SIMD parallelization */	
        _dp_h[i] = -gap_ext1 * i;	                                    
    }

    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {
        //simd_abpoa_lg_dp(int16_t, SIMDShiftOneNi16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDGetIfGreater, SIMDSetIfGreater);        
        node_id = abpoa_graph_index_to_node_id(graph, index_i);	
        SIMDi *q = qp + graph->node[node_id].base * qp_sn;
        dp_h = &DP_H[index_i * dp_sn]; _dp_h = (int16_t*)dp_h;	                                                    
        int min_pre_beg_sn, max_pre_end_sn;   
        if (abpt->bw < 0) { 
            beg = dp_beg[index_i] = 0, end = dp_end[index_i] = qlen;	                                                        
            beg_sn = dp_beg_sn[index_i] = (dp_beg[index_i])/pn; end_sn = dp_end_sn[index_i] = (dp_end[index_i])/pn; 
            min_pre_beg_sn = 0, max_pre_end_sn = end_sn;    
        } else {  
            beg = GET_AD_DP_BEGIN(graph, w, node_id, qlen), end = GET_AD_DP_END(graph, w, node_id, qlen);   
            beg_sn = beg / pn; min_pre_beg_sn = INT32_MAX, max_pre_end_sn = -1; 
            for (i = 0; i < pre_n[index_i]; ++i) {  
                pre_i = pre_index[index_i][i];  
                if (min_pre_beg_sn > dp_beg_sn[pre_i]) min_pre_beg_sn = dp_beg_sn[pre_i];   
                if (max_pre_end_sn < dp_end_sn[pre_i]) max_pre_end_sn = dp_end_sn[pre_i];   
            } if (beg_sn < min_pre_beg_sn) beg_sn = min_pre_beg_sn; 
            dp_beg_sn[index_i] = beg_sn; beg = dp_beg[index_i] = dp_beg_sn[index_i] * pn;   
            end_sn = dp_end_sn[index_i] = end/pn; end = dp_end[index_i] = (dp_end_sn[index_i]+1)*pn-1;  
        }   
        /* loop query */
        /* init h, e */
        _beg_sn = MAX_OF_TWO(beg_sn-1, 0); _end_sn = MIN_OF_TWO(end_sn+1, dp_sn-1);
        for (i = _beg_sn; i <= _end_sn; ++i) { /* SIMD parallelization */
            dp_h[i] = SIMD_MIN_INF;
        }	                      
#ifdef __DEBUG__
        simd_abpoa_print_lg_matrix_row(1, int16_t, index_i);
#endif
        /* get max m and e */	 
        for (i = 0; i < pre_n[index_i]; ++i) {
            pre_i = pre_index[index_i][i];
            pre_dp_h = DP_H + pre_i * dp_sn;
            pre_end = dp_end[pre_i];	   
            pre_beg_sn = dp_beg_sn[pre_i];
            /* set M from (pre_i, q_i-1), E from (pre_i, q_i) */
            _beg_sn = MAX_OF_THREE(0, pre_beg_sn, (beg-1)/pn); _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);
            SIMDi first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneNi16);	                          
            for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */	                             
                SIMDi remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneNi16);	                                    
                dp_h[sn_i] = SIMDMaxi16(SIMDAddi16(SIMDOri(first, remain), q[sn_i]), SIMDMaxi16(SIMDSubi16(pre_dp_h[sn_i], GAP_E1), dp_h[sn_i]));
                first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneNi16);	                                
            } /* now we have max(h,e) stored at dp_h */	                                                                   
        }	                                                                                                              
#ifdef __DEBUG__
        simd_abpoa_print_lg_matrix_row(2, int16_t, index_i);
#endif
        /* new F start */   
        SIMDi first = SIMDOri(SIMDAndi(dp_h[beg_sn], PRE_MASK[0]), SUF_MIN[0]);   
        for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) {   
            if (sn_i < min_pre_beg_sn) {
                _err_fatal_simple(__func__, "sn_i < min_pre_beg_sn\n"); 
            } else if (sn_i > max_pre_end_sn) {
                set_num = sn_i == max_pre_end_sn+1 ? 1 : 0;
            } else set_num = pn;
            dp_h[sn_i] = SIMDMaxi16(dp_h[sn_i], first);    
#ifdef __DEBUG__
            simd_abpoa_print_lg_matrix_row(4, int16_t, index_i);
#endif
            SIMD_SET_F(dp_h[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16);    
#ifdef __DEBUG__
            simd_abpoa_print_lg_matrix_row(5, int16_t, index_i);
#endif
            first = SIMDOri(SIMDAndi(SIMDShiftRight(SIMDSubi16(dp_h[sn_i], GAP_E1), SIMDTotalBytes-SIMDShiftOneNi16), PRE_MASK[0]), SUF_MIN[0]);    
        }  
#ifdef __DEBUG__
        simd_abpoa_print_lg_matrix_row(3, int16_t, index_i);
#endif

        if (abpt->bw >= 0) {                                                                                            
            //simd_abpoa_max(int16_t, SIMDSetIfGreater, SIMDGetIfGreater);                                            
            /* select max dp_h */                                               
            max = inf_min, max_i = -1;                                          
            SIMDi a = dp_h[end_sn], b = qi[end_sn];                             
            if (end_sn == qlen / pn) SIMDSetIfGreateri16(a, zero, b, SIMD_MIN_INF, a);  
            for (i = beg_sn; i < end_sn; ++i) {                                 
                SIMDGetIfGreateri16(b, a, dp_h[i], a, qi[i], b);                   
            }                                                                   
            _dp_h = (int16_t*)&a, _qi = (int16_t*)&b;                           
            for (i = 0; i < pn; ++i) {                                          
                if (_dp_h[i] > max) {                                           
                    max = _dp_h[i]; max_i = _qi[i];                             
                }                                                               
            }                                                                   

            //simd_abpoa_ada_max_i; 
            /* set min/max_rank for next nodes */                                                                                       
            int max_out_i = max_i + 1, min_out_i = max_i + 1;                                                                           
            for (i = 0; i < graph->node[node_id].out_edge_n; ++i) {                                                                     
                int out_node_id = graph->node[node_id].out_id[i];                                                                       
                if (max_out_i > graph->node_id_to_max_rank[out_node_id]) graph->node_id_to_max_rank[out_node_id] = max_out_i;     
                if (min_out_i < graph->node_id_to_min_rank[out_node_id]) graph->node_id_to_min_rank[out_node_id] = min_out_i;     
            }                                                                                                                           
        }                                                                                                               
    }                                                                                                                   
    // printf("dp_sn: %d, node_n: %d, seq_n: %d\n", tot_dp_sn, graph->node_n, qlen);   
    //simd_abpoa_global_get_max(int16_t, DP_H2E, 3*dp_sn);                                                                
    int in_id, in_index;	                                                        
    for (i = 0; i < graph->node[ABPOA_SINK_NODE_ID].in_edge_n; ++i) {	            
        in_id = graph->node[ABPOA_SINK_NODE_ID].in_id[i];	                        
        in_index = abpoa_graph_node_id_to_index(graph, in_id);	                    
        dp_h = DP_H + in_index * dp_sn;	                                        
        _dp_h = (int16_t*)dp_h;	                                                    
        set_max_score_global(best_score, best_i, best_j, _dp_h[qlen], in_index, qlen);    
    }	                                                                            

    simd_abpoa_print_lg_matrix(int16_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score);        
    //simd_abpoa_lg_get_cigar(int16_t);                                                                                   
    int m = abpt->m, n_c = 0, s, m_c = 0, id, hit, _start_i, _start_j;                                                       
    int16_t *_pre_dp_h;                                                                
    abpoa_cigar_t *cigar = 0;                                                                                               
    i = best_i, j = best_j, _start_i = best_i, _start_j = best_j, id = abpoa_graph_index_to_node_id(graph, i);                                                    
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);                   
    dp_h = DP_H + i * dp_sn; _dp_h = (int16_t*)dp_h;                                                                        
    while (i > 0 && j > 0) {                                                                    
        _start_i = i, _start_j = j;                                                                         
        int *pre_index_i = pre_index[i];                                                                    
        s = mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            
        for (k = 0; k < pre_n[i]; ++k) {                                                                    
            pre_i = pre_index_i[k];                                                                         
            pre_dp_h = DP_H + pre_i * dp_sn; _pre_dp_h = (int16_t*)pre_dp_h;                                
            if (_pre_dp_h[j-1] + s == _dp_h[j]) { /* match/mismatch */                                      
                cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                      
                i = pre_i; --j; hit = 1; id = abpoa_graph_index_to_node_id(graph, i);                       
                dp_h = DP_H + i * dp_sn; _dp_h = (int16_t*)dp_h;                                            
                break;                                                                                      
            }                                                                                               
        }                                                                                                   
        if (hit == 0) {                                                                                     
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                pre_dp_h = DP_H + pre_i * dp_sn; _pre_dp_h = (int16_t*)pre_dp_h;                            
                if (_pre_dp_h[j] - gap_ext1 == _dp_h[j]) { /* deletion */                                     
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                    
                    i = pre_i; hit = 1; id = abpoa_graph_index_to_node_id(graph, i);                        
                    dp_h = DP_H + i * dp_sn; _dp_h = (int16_t*)dp_h;                                        
                    break;                                                                                  
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0 && _dp_h[j-1] - gap_ext1 == _dp_h[j]) { /* insertion */                                  
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); j--;                       
        }                                                                                                   
    }
    if (j > 0) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j, -1, j-1);                     
    /* reverse cigar */                                                                                                     
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                                                                         
    res->n_cigar = n_c;                                                                                                         
    res->node_s = _start_i, res->query_s = _start_j-1;
    res->node_e = best_i, res->query_e = best_j-1;
    /*abpoa_print_cigar(n_c, *graph_cigar, graph);*/                                                                        
    simd_abpoa_free_var; free(GAP_E1S);
    return best_score;
}

int abpoa_ag_global_align_sequence_to_graph_core(abpoa_t *ab, int qlen, uint8_t *query, abpoa_para_t *abpt, SIMD_para_t sp, abpoa_res_t *res) {
    //simd_abpoa_cg_var(int16_t, sp, SIMDSetOnei16);                                                                         
    //simd_abpoa_var(int16_t, sp, SIMDSetOnei16);        
    //int tot_dp_sn = 0;  
    abpoa_graph_t *graph = ab->abg; abpoa_simd_matrix_t *abm = ab->abm;                             
    int matrix_row_n = graph->node_n, matrix_col_n = qlen + 1;                                      
    int **pre_index, *pre_n, pre_i;                                                                 
    int i, j, k, *dp_beg, *dp_beg_sn, *dp_end, *dp_end_sn, node_id, index_i;          
    int beg, end, beg_sn, end_sn, _beg_sn, _end_sn, pre_beg_sn, pre_end, sn_i;
    int pn, log_n, size, qp_sn, dp_sn; /* pn: value per SIMDi, qp_sn/dp_sn/d_sn: segmented length*/              
    SIMDi *dp_h, *pre_dp_h, *qp, *qi=NULL;                                                   
    int16_t *_dp_h=NULL, *_qi, best_score = sp.inf_min, inf_min = sp.inf_min;               
    int *mat = abpt->mat, best_i = 0, best_j = 0, max, max_i; int16_t gap_ext1 = abpt->gap_ext1;    
    int w = abpt->bw < 0 ? qlen : abpt->bw; /* when w < 0, do whole global */                       
    SIMDi zero = SIMDSetZeroi(), SIMD_MIN_INF = SIMDSetOnei16(inf_min);                                     
    pn = sp.num_of_value; qp_sn = dp_sn = (matrix_col_n + pn - 1) / pn, log_n = sp.log_num, size = sp.size; 
    qp = abm->s_mem;                                                                                
    // for SET_F mask[pn], suf_min[pn], pre_min[logN]
    int set_num; SIMDi *PRE_MASK, *SUF_MIN, *PRE_MIN;
    PRE_MASK = (SIMDi*)SIMDMalloc((pn+1) * size, size), SUF_MIN = (SIMDi*)SIMDMalloc((pn+1) * size, size), PRE_MIN = (SIMDi*)SIMDMalloc(pn * size, size);
    for (i = 0; i < pn; ++i) {
        int16_t *pre_mask = (int16_t*)(PRE_MASK + i);
        for (j = 0; j <= i; ++j) pre_mask[j] = -1;
        for (j = i+1; j < pn; ++j) pre_mask[j] = 0;
    } PRE_MASK[pn] = PRE_MASK[pn-1];
    SUF_MIN[0] = SIMDShiftLeft(SIMD_MIN_INF, SIMDShiftOneNi16); 
    for (i = 1; i < pn; ++i) SUF_MIN[i] = SIMDShiftLeft(SUF_MIN[i-1], SIMDShiftOneNi16); SUF_MIN[pn] = SUF_MIN[pn-1];
    for (i = 1; i < pn; ++i) {
        int16_t *pre_min = (int16_t*)(PRE_MIN + i);
        for (j = 0; j < i; ++j) pre_min[j] = inf_min;
        for (j = i; j < pn; ++j) pre_min[j] = 0;
    }
    // simd_abpoa_ag_only_var(int16_t, SIMDSetOnei16);    
    int16_t *_dp_e, gap_open1 = abpt->gap_open1, gap_oe = abpt->gap_open1 + abpt->gap_ext1;         
    int pre_end_sn;                                                                                 
    SIMDi *DP_HE, *dp_e, *pre_dp_e, GAP_O1 = SIMDSetOnei16(gap_open1), GAP_E1 = SIMDSetOnei16(gap_ext1), GAP_OE1 = SIMDSetOnei16(gap_oe);      
    DP_HE = qp + qp_sn * abpt->m; SIMDi *dp_f = DP_HE + ((dp_sn * matrix_row_n) << 1);                     
    SIMDi *GAP_E1S =  (SIMDi*)SIMDMalloc(log_n * size, size);      
    GAP_E1S[0] = GAP_E1;  
    for (i = 1; i < log_n; ++i) {   
        GAP_E1S[i] = SIMDAddi16(GAP_E1S[i-1], GAP_E1S[i-1]);   
    }   
    
    // simd_abpoa_init_var(int16_t, sp);
    /* generate the query profile */                                                                                        
    for (i = 0; i < qp_sn * abpt->m; ++i) qp[i] = SIMD_MIN_INF;                                                                  
    for (k = 0; k < abpt->m; ++k) { /* SIMD parallelization */                                                              
        int *p = &mat[k * abpt->m];                                                                                         
        int16_t *_qp = (int16_t*)(qp + k * qp_sn); _qp[0] = 0;                                                              
        for (j = 0; j < qlen; ++j) _qp[j+1] = (int16_t)p[query[j]];                                                         
        for (j = qlen+1; j < qp_sn * pn; ++j) _qp[j] = 0;                                                                   
    }                                                                                                                       
    if (abpt->bw >= 0 || abpt->align_mode == ABPOA_EXTEND_MODE) { /* query index */ 
        qi = dp_f + dp_sn; _qi = (int16_t*)qi;                                                                              
        for (i = 0; i <= qlen; ++i) _qi[i] = i;                                                                             
        for (i = qlen+1; i < (qlen/pn+1) * pn; ++i) _qi[i] = -1;                                                            
    }                                                                                                                       
    /* for backtrack */                                                                                                     
    dp_beg = abm->dp_beg, dp_end = abm->dp_end, dp_beg_sn = abm->dp_beg_sn, dp_end_sn = abm->dp_end_sn;                     
    if (abpt->bw >= 0) {                                                                                                    
        for (i = 0; i < graph->node_n; ++i) {                                                                               
            // node_id = graph->index_to_node_id[i];                                                                           
            graph->node_id_to_min_rank[i] = graph->node_n;                                                            
            graph->node_id_to_max_rank[i] = 0;                                                                        
        }                                                                                                                   
    }                                                                                                                       
    /* index of pre-node */                                                                                                 
    pre_index = (int**)_err_malloc(graph->node_n * sizeof(int*));                                                           
    pre_n = (int*)_err_malloc(graph->node_n * sizeof(int*));                                                                
    for (i = 0; i < graph->node_n; ++i) {                                                                                   
        node_id = abpoa_graph_index_to_node_id(graph, i); /* i: node index */                                               
        pre_n[i] = graph->node[node_id].in_edge_n;                                                                          
        pre_index[i] = (int*)_err_malloc(pre_n[i] * sizeof(int));                                                           
        for (j = 0; j < pre_n[i]; ++j) {                                                                                    
            pre_index[i][j] = abpoa_graph_node_id_to_index(graph, graph->node[node_id].in_id[j]);                           
        }                                                                                                                   
    }                                                                                                                       
    //simd_abpoa_ag_first_dp(int16_t);                                                                                    
    //simd_abpoa_ag_first_row;                                                    
    /* fill the first row */                                                                    
    if (abpt->bw >= 0) {                                                                                
        graph->node_id_to_min_rank[0] = graph->node_id_to_max_rank[0] = 0;                              
        for (i = 0; i < graph->node[0].out_edge_n; ++i) { /* set min/max rank for next_id */            
            int out_id = graph->node[0].out_id[i];                                                      
            graph->node_id_to_min_rank[out_id] = graph->node_id_to_max_rank[out_id] = 1;                
        }                                                                                               
        dp_beg[0] = GET_AD_DP_BEGIN(graph, w, 0, qlen), dp_end[0] = GET_AD_DP_END(graph, w, 0, qlen);   
    } else {                                                                                            
        dp_beg[0] = 0, dp_end[0] = qlen;                                                                
    }                                                                                                   
    dp_beg_sn[0] = (dp_beg[0])/pn; dp_end_sn[0] = (dp_end[0])/pn;                                       
    dp_beg[0] = dp_beg_sn[0] * pn; dp_end[0] = (dp_end_sn[0]+1)*pn-1;                                   
    dp_h = DP_HE; dp_e = dp_h + dp_sn;                                                                  
    _end_sn = MIN_OF_TWO(dp_end_sn[0]+1, dp_sn-1);   

    _dp_h = (int16_t*)dp_h, _dp_e = (int16_t*)dp_e;                     
    for (i = 0; i <= _end_sn; ++i) {                                    
        dp_h[i] = SIMD_MIN_INF; dp_e[i] = SIMD_MIN_INF;                           
    }                                                                   
    for (i = 1; i <= dp_end[0]; ++i) { /* no SIMD parallelization */    
        _dp_h[i] = -gap_open1 - gap_ext1 * i;                           
    } _dp_h[0] = 0; _dp_e[0] = -(gap_oe);                               
                 

    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {
        //simd_abpoa_ag_dp(int16_t, SIMDShiftOneNi16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDGetIfGreater, SIMDSetIfGreater);        
        node_id = abpoa_graph_index_to_node_id(graph, index_i); 
    SIMDi *q = qp + graph->node[node_id].base * qp_sn;                                                                                  
    dp_h = DP_HE + (index_i<<1) * dp_sn; dp_e = dp_h + dp_sn;                                                                           
    _dp_h = (int16_t*)dp_h, _dp_e = (int16_t*)dp_e;                                                             
    int min_pre_beg_sn, max_pre_end_sn;   
    if (abpt->bw < 0) { 
        beg = dp_beg[index_i] = 0, end = dp_end[index_i] = qlen;    
        beg_sn = dp_beg_sn[index_i] = (dp_beg[index_i])/pn; end_sn = dp_end_sn[index_i] = (dp_end[index_i])/pn; 
        min_pre_beg_sn = 0, max_pre_end_sn = end_sn;    
    } else {    
        beg = GET_AD_DP_BEGIN(graph, w, node_id, qlen), end = GET_AD_DP_END(graph, w, node_id, qlen);   
        beg_sn = beg / pn; min_pre_beg_sn = INT32_MAX, max_pre_end_sn = -1; 
        for (i = 0; i < pre_n[index_i]; ++i) {  
            pre_i = pre_index[index_i][i];  
            if (min_pre_beg_sn > dp_beg_sn[pre_i]) min_pre_beg_sn = dp_beg_sn[pre_i];   
            if (max_pre_end_sn < dp_end_sn[pre_i]) max_pre_end_sn = dp_end_sn[pre_i];   
        } if (beg_sn < min_pre_beg_sn) beg_sn = min_pre_beg_sn; 
        dp_beg_sn[index_i] = beg_sn; beg = dp_beg[index_i] = dp_beg_sn[index_i] * pn;   
        end_sn = dp_end_sn[index_i] = end/pn; end = dp_end[index_i] = (dp_end_sn[index_i]+1)*pn-1;  
    }   
    /* loop query */                                                                                                                    
    /* init h, e */                                                                                                                     
    _beg_sn = MAX_OF_TWO(beg_sn-1, 0); _end_sn = MIN_OF_TWO(end_sn+1, dp_sn-1);                                                         
    for (i = _beg_sn; i <= _end_sn; ++i) { /* SIMD parallelization */                                                                   
        dp_h[i] = SIMD_MIN_INF; dp_e[i] = SIMD_MIN_INF; dp_f[i] = SIMD_MIN_INF;                                                                        
    }                                                                                                                                   
    /* get max m and e */                                                                                                               
    for (i = 0; i < pre_n[index_i]; ++i) {                                                                                              
        pre_i = pre_index[index_i][i];                                                                                                  
        pre_dp_h = DP_HE + (pre_i << 1) * dp_sn; pre_dp_e = pre_dp_h + dp_sn;                                                           
        pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];                                          
        /* set M from (pre_i, q_i-1) */                                                                                                 
        _beg_sn = MAX_OF_THREE(0, pre_beg_sn, (beg-1)/pn);                                                                              
        _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);                                                                        
        SIMDi first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneNi16); 
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                                      
            SIMDi remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneNi16);                                                                
            dp_h[sn_i] = SIMDMaxi16(SIMDOri(first, remain), dp_h[sn_i]);                                                                   
            first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneNi16);                                                       
        }                                                                                                                               
        /* set E from (pre_i, q_i) */                                                                                                   
        _beg_sn = MAX_OF_TWO(pre_beg_sn, beg_sn); _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);                                             
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */                                                      
            dp_e[sn_i] = SIMDMaxi16(pre_dp_e[sn_i], dp_e[sn_i]);                                                                           
        }                                                                                                                               
    }                                                                                                                                   
    /* compare M, E, and F */                                                                                                           
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */                                                            
        dp_h[sn_i] = SIMDMaxi16(SIMDAddi16(dp_h[sn_i], q[sn_i]), dp_e[sn_i]); 
    }                                                                                                                                   
    /* new F start */   
    SIMDi first = SIMDShiftRight(SIMDShiftLeft(dp_h[beg_sn], SIMDTotalBytes-SIMDShiftOneNi16), SIMDTotalBytes-SIMDShiftOneNi16);
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { 
        if (sn_i < min_pre_beg_sn) {
            _err_fatal_simple(__func__, "sn_i < min_pre_beg_sn\n"); 
        } else if (sn_i > max_pre_end_sn) {
            set_num = sn_i == max_pre_end_sn+1 ? 2 : 1;
        } else set_num = pn;
        /* F = (H << 1 | x) - OE */
        dp_f[sn_i] = SIMDSubi16(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneNi16), first), GAP_OE1);
        /* F = max{F, (F-e)<<1}, F = max{F, (F-2e)<<2} ... */
        SIMD_SET_F(dp_f[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16);
        /* x = max{H, F+o} */
        first = SIMDShiftRight(SIMDMaxi16(dp_h[sn_i], SIMDAddi16(dp_f[sn_i], GAP_O1)), SIMDTotalBytes-SIMDShiftOneNi16);
        /* H = max{H, F} */
        dp_h[sn_i] = SIMDMaxi16(dp_h[sn_i], dp_f[sn_i]);                                                                                   
    }
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */                                                            
        /* e for next cell */                                                                                                           
        dp_e[sn_i] = SIMDMaxi16(SIMDSubi16(dp_e[sn_i], GAP_E1), SIMDSubi16(dp_h[sn_i], GAP_OE1)); 
    }                                                                                                                                   
                                                                                                                      
        if (abpt->bw >= 0) {                                                                                            
            //simd_abpoa_max(int16_t, SIMDSetIfGreater, SIMDGetIfGreater);                                            
            /* select max dp_h */                                               
            max = inf_min, max_i = -1;                                          
            SIMDi a = dp_h[end_sn], b = qi[end_sn];                             
            if (end_sn == qlen / pn) SIMDSetIfGreateri16(a, zero, b, SIMD_MIN_INF, a);  
            for (i = beg_sn; i < end_sn; ++i) {                                 
                SIMDGetIfGreateri16(b, a, dp_h[i], a, qi[i], b);                   
            }                                                                   
            _dp_h = (int16_t*)&a, _qi = (int16_t*)&b;                           
            for (i = 0; i < pn; ++i) {                                          
                if (_dp_h[i] > max) {                                           
                    max = _dp_h[i]; max_i = _qi[i];                             
                }                                                               
            }                                                                   

            //simd_abpoa_ada_max_i; 
            /* set min/max_rank for next nodes */                                                                                       
            int max_out_i = max_i + 1, min_out_i = max_i + 1;                                                                           
            for (i = 0; i < graph->node[node_id].out_edge_n; ++i) {                                                                     
                int out_node_id = graph->node[node_id].out_id[i];                                                                       
                if (max_out_i > graph->node_id_to_max_rank[out_node_id]) graph->node_id_to_max_rank[out_node_id] = max_out_i;     
                if (min_out_i < graph->node_id_to_min_rank[out_node_id]) graph->node_id_to_min_rank[out_node_id] = min_out_i;     
            }                                                                                                                           
        }                                                                                                               
    }                                                                                                                   
    // printf("dp_sn: %d, node_n: %d, seq_n: %d\n", tot_dp_sn, graph->node_n, qlen);   
    //simd_abpoa_global_get_max(int16_t, DP_H2E, 3*dp_sn);                                                                
    int in_id, in_index;	                                                        
    for (i = 0; i < graph->node[ABPOA_SINK_NODE_ID].in_edge_n; ++i) {	            
        in_id = graph->node[ABPOA_SINK_NODE_ID].in_id[i];	                        
        in_index = abpoa_graph_node_id_to_index(graph, in_id);	                    
        dp_h = DP_HE + in_index * 2 * dp_sn;	                                        
        _dp_h = (int16_t*)dp_h;	                                                    
        set_max_score_global(best_score, best_i, best_j, _dp_h[qlen], in_index, qlen);    
    }	                                                                            

    // simd_abpoa_print_ag_matrix(int16_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score);        
    //simd_abpoa_ag_get_cigar(int16_t);                                                                                   
    int m = abpt->m, n_c = 0, s, m_c = 0, id, hit, _start_i, _start_j, cur_op = ABPOA_ALL_OP;                                                       
    int16_t *_pre_dp_h, *_pre_dp_e; abpoa_cigar_t *cigar = 0;                                                                               
    i = best_i, j = best_j, _start_i = best_i, _start_j = best_j, id = abpoa_graph_index_to_node_id(graph, i);                                    
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);   
    dp_h = DP_HE + dp_sn * (i << 1); _dp_h = (int16_t*)dp_h;                                                

    while (i > 0 && j > 0) {                                                                    
        _start_i = i, _start_j = j;                                                                         
        int *pre_index_i = pre_index[i];                                                                    
        s = mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            
        if (cur_op & ABPOA_M_OP) {                                                                          
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                /* match/mismatch */                                                                        
                pre_dp_h = DP_HE + dp_sn * (pre_i << 1); _pre_dp_h = (int16_t*)pre_dp_h;                    
                if (_pre_dp_h[j-1] + s == _dp_h[j]) {                                                       
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                  
                    i = pre_i; --j; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                   
                    dp_h = DP_HE + dp_sn * (i << 1); _dp_h = (int16_t*)dp_h;                                
                    cur_op = ABPOA_ALL_OP;                                                                  
                    break;                                                                                  
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0 && cur_op & ABPOA_E1_OP) {                                                             
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                /* deletion */                                                                              
                pre_dp_e = DP_HE + dp_sn * ((pre_i << 1) + 1); _pre_dp_e = (int16_t*)pre_dp_e;              
                if (_pre_dp_e[j] == _dp_h[j]) {                                                             
                    pre_dp_h = DP_HE + dp_sn * (pre_i << 1); _pre_dp_h = (int16_t*)pre_dp_h;                
                    if (_pre_dp_h[j] - gap_ext1 == _pre_dp_e[j]) cur_op = ABPOA_E1_OP;                         
                    else cur_op = ABPOA_M_OP;                                                               
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                    
                    i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                        
                    dp_h = DP_HE + dp_sn * (i << 1); _dp_h = (int16_t*)dp_h;                                
                    break;                                                                                  
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0 && cur_op & ABPOA_F1_OP) { /* insertion */                                             
            if (_dp_h[j-1] - gap_ext1 == _dp_h[j]) cur_op = ABPOA_F1_OP;                                       
            else cur_op = ABPOA_M_OP; /*if (_dp_h[j-1] - gap_oe == _dp_h[j]) */                             
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); --j;                       
        }                                                                                                   
    }
    if (j > 0) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j, -1, j-1);     
    /* reverse cigar */                                                                                     
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                                                         
    res->n_cigar = n_c;                                                                                         
    res->node_s = _start_i, res->query_s = _start_j-1;
    res->node_e = best_i, res->query_e = best_j-1;
    /*abpoa_print_cigar(n_c, *graph_cigar, graph);*/                                                        
 
    simd_abpoa_free_var; free(GAP_E1S);
    return best_score;
}

void _simd_abpoa_init_var(abpoa_para_t *abpt, uint8_t *query, int qlen, SIMDi *qp, SIMDi *dp_f, SIMDi **qi, int *mat, int qp_sn, int dp_sn, int pn, SIMDi SIMD_MIN_INF) {
    int i, j, k; int16_t *_qi;
    /* generate the query profile */
    for (i = 0; i < qp_sn * abpt->m; ++i) qp[i] = SIMD_MIN_INF;
    for (k = 0; k < abpt->m; ++k) { /* SIMD parallelization */
        int *p = &mat[k * abpt->m];
        int16_t *_qp = (int16_t*)(qp + k * qp_sn); _qp[0] = 0;
        for (j = 0; j < qlen; ++j) _qp[j+1] = (int16_t)p[query[j]];
        for (j = qlen+1; j < qp_sn * pn; ++j) _qp[j] = 0;
    }                                     
    if (abpt->bw >= 0 || abpt->align_mode == ABPOA_EXTEND_MODE) { /* query index */ 
        *qi = dp_f + dp_sn;
        _qi = (int16_t*)*qi;                                                                              
        for (i = 0; i <= qlen; ++i) _qi[i] = i;
        for (i = qlen+1; i < (qlen/pn+1) * pn; ++i) _qi[i] = -1;
    }
}

void _simd_abpoa_cg_first_dp(abpoa_para_t *abpt, abpoa_graph_t *graph, int *dp_beg, int *dp_end, int *dp_beg_sn, int *dp_end_sn, int pn, int qlen, int w, int dp_sn, SIMDi *DP_H2E, SIMDi SIMD_MIN_INF, int gap_open1, int gap_ext1, int gap_open2, int gap_ext2, int gap_oe1, int gap_oe2) {                                           
    int i, _end_sn;
    if (abpt->bw >= 0) {                                                                                
        graph->node_id_to_min_rank[0] = graph->node_id_to_max_rank[0] = 0;                              
        for (i = 0; i < graph->node[0].out_edge_n; ++i) { /* set min/max rank for next_id */            
            int out_id = graph->node[0].out_id[i];                                                      
            graph->node_id_to_min_rank[out_id] = graph->node_id_to_max_rank[out_id] = 1;                
        }                                                                                               
        dp_beg[0] = 0, dp_end[0] = w; // GET_AD_DP_BEGIN(graph, w, 0, qlen), dp_end[0] = GET_AD_DP_END(graph, w, 0, qlen);   
    } else {                                                                                            
        dp_beg[0] = 0, dp_end[0] = qlen;                                                                
    }                                                                                                   
    dp_beg_sn[0] = (dp_beg[0])/pn; dp_end_sn[0] = (dp_end[0])/pn;                                       
    dp_beg[0] = dp_beg_sn[0] * pn; dp_end[0] = (dp_end_sn[0]+1)*pn-1;                                   
    SIMDi *dp_h = DP_H2E; SIMDi *dp_e1 = dp_h + dp_sn; SIMDi *dp_e2 = dp_e1 + dp_sn;                                         
    _end_sn = MIN_OF_TWO(dp_end_sn[0]+1, dp_sn-1);

    for (i = 0; i <= _end_sn; ++i) {
        dp_h[i] = SIMD_MIN_INF; dp_e1[i] = SIMD_MIN_INF; dp_e2[i] = SIMD_MIN_INF;   
    }                                                                               
    int16_t *_dp_h = (int16_t*)dp_h, *_dp_e1 = (int16_t*)dp_e1, *_dp_e2 = (int16_t*)dp_e2;     
    _dp_h[0] = 0; _dp_e1[0] = -(gap_oe1); _dp_e2[0] = -(gap_oe2);                 
    for (i = 1; i <= dp_end[0]; ++i) { /* no SIMD parallelization */                
        _dp_h[i] = -MIN_OF_TWO(gap_open1+gap_ext1*i, gap_open2+gap_ext2*i);         
    }
}

int _simd_abpoa_max(SIMDi SIMD_MIN_INF, SIMDi zero, int inf_min, SIMDi *dp_h, SIMDi *qi, int qlen, int pn, int beg_sn, int end_sn) {
    /* select max dp_h */                                                   
    int max = inf_min, max_i = -1, i;                                              
    SIMDi a = dp_h[end_sn], b = qi[end_sn];                                 
    if (end_sn == qlen / pn) SIMDSetIfGreateri16(a, zero, b, SIMD_MIN_INF, a); 
    for (i = beg_sn; i < end_sn; ++i) {                                     
        SIMDGetIfGreateri16(b, a, dp_h[i], a, qi[i], b);                       
    }                                                                       
    int16_t *_dp_h = (int16_t*)&a, *_qi = (int16_t*)&b;                               
    for (i = 0; i < pn; ++i) {                                              
        if (_dp_h[i] > max) {                                               
            max = _dp_h[i]; max_i = _qi[i];                                 
        }                                                                   
    }                                                                       
    return max_i;
}

void _simd_abpoa_ada_max_i(int max_i, abpoa_graph_t *graph, int node_id) {
    /* set min/max_rank for next nodes */                                                                       
    int out_i = max_i + 1, i;                                                                                      
    for (i = 0; i < graph->node[node_id].out_edge_n; ++i) {                                                     
        int out_node_id = graph->node[node_id].out_id[i];                                                       
        if (out_i > graph->node_id_to_max_rank[out_node_id]) graph->node_id_to_max_rank[out_node_id] = out_i;   
        if (out_i < graph->node_id_to_min_rank[out_node_id]) graph->node_id_to_min_rank[out_node_id] = out_i;   
    }                                                                                                           
}

void _simd_abpoa_global_get_max(abpoa_graph_t *graph, SIMDi *DP_H_HE, int dp_sn, int qlen, int16_t *best_score, int *best_i, int *best_j) {	                    
    int in_id, in_index, i;	                                                        
    for (i = 0; i < graph->node[ABPOA_SINK_NODE_ID].in_edge_n; ++i) {	            
        in_id = graph->node[ABPOA_SINK_NODE_ID].in_id[i];	                        
        in_index = abpoa_graph_node_id_to_index(graph, in_id);	                    
        SIMDi *dp_h = DP_H_HE + in_index * dp_sn;	                                        
        int16_t *_dp_h = (int16_t*)dp_h;	                                                    
        // set_max_score(*best_score, *best_i, *best_j, _dp_h[qlen], in_index, qlen);    
        if (_dp_h[qlen] > *best_score) {                                     
            *best_score = _dp_h[qlen]; *best_i = in_index; *best_j = qlen;               
        }                                                             
    }	                                                                            
}

int _simd_abpoa_cg_dp(SIMDi *q, SIMDi *dp_h, SIMDi *dp_e1, SIMDi *dp_e2, int index_i, abpoa_graph_t *graph, abpoa_para_t *abpt, int dp_sn, int pn, int qlen, int w, SIMDi *DP_H2E, SIMDi *dp_f, SIMDi *dp_f2, SIMDi SIMD_MIN_INF, SIMDi GAP_O1, SIMDi GAP_O2, SIMDi GAP_E1, SIMDi GAP_E2, SIMDi GAP_OE1, SIMDi GAP_OE2, SIMDi* GAP_E1S, SIMDi* GAP_E2S, SIMDi *PRE_MIN, SIMDi *PRE_MASK, SIMDi *SUF_MIN, int log_n, int *dp_beg, int *dp_end, int *dp_beg_sn, int *dp_end_sn, int *pre_n, int **pre_index) {
    int tot_dp_sn = 0, i, pre_i, node_id = abpoa_graph_index_to_node_id(graph, index_i);
    int min_pre_beg_sn, max_pre_end_sn, beg, end, beg_sn, end_sn, pre_end, pre_end_sn, pre_beg_sn, sn_i;
    if (abpt->bw < 0) {  
        beg = dp_beg[index_i] = 0, end = dp_end[index_i] = qlen;
        beg_sn = dp_beg_sn[index_i] = beg/pn; end_sn = dp_end_sn[index_i] = end/pn;
        min_pre_beg_sn = 0, max_pre_end_sn = end_sn;
    } else {
        beg = GET_AD_DP_BEGIN(graph, w, node_id, qlen), end = GET_AD_DP_END(graph, w, node_id, qlen);
        beg_sn = beg / pn; min_pre_beg_sn = INT32_MAX, max_pre_end_sn = -1;
        for (i = 0; i < pre_n[index_i]; ++i) {
            pre_i = pre_index[index_i][i];
            if (min_pre_beg_sn > dp_beg_sn[pre_i]) min_pre_beg_sn = dp_beg_sn[pre_i];
            if (max_pre_end_sn < dp_end_sn[pre_i]) max_pre_end_sn = dp_end_sn[pre_i];
        } if (beg_sn < min_pre_beg_sn) beg_sn = min_pre_beg_sn;
        dp_beg_sn[index_i] = beg_sn; beg = dp_beg[index_i] = dp_beg_sn[index_i] * pn;
        end_sn = dp_end_sn[index_i] = end/pn; end = dp_end[index_i] = (dp_end_sn[index_i]+1)*pn-1;
    }
    tot_dp_sn += (end_sn - beg_sn + 1);
    /* loop query */
    // new init start
    int _beg_sn, _end_sn;
    // first pre_node
    pre_i = pre_index[index_i][0];
    SIMDi *pre_dp_h = DP_H2E + (pre_i * 3) * dp_sn; SIMDi *pre_dp_e1 = pre_dp_h + dp_sn; SIMDi *pre_dp_e2 = pre_dp_e1 + dp_sn;
    pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];
    SIMDi first, remain;
    /* set M from (pre_i, q_i-1) */
    if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneNi16);
    else _beg_sn = pre_end_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneNi16);
    _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */
        remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneNi16);
        // dp_h[sn_i] = SIMDOri(first, remain);
        SIMDStorei(&dp_h[sn_i], SIMDOri(first, remain));
        first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneNi16);
    }
    /* set E from (pre_i, q_i) */
    _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);
    for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */
        //dp_e1[sn_i] = pre_dp_e1[sn_i];                                                                    
        SIMDStorei(&dp_e1[sn_i], pre_dp_e1[sn_i]);
        // dp_e2[sn_i] = pre_dp_e2[sn_i];                                                                    
        SIMDStorei(&dp_e2[sn_i], pre_dp_e2[sn_i]);                                                                    
    }
    // new init end
    /* get max m and e */
    for (i = 1; i < pre_n[index_i]; ++i) {
        pre_i = pre_index[index_i][i];
        pre_dp_h = DP_H2E + (pre_i * 3) * dp_sn; pre_dp_e1 = pre_dp_h + dp_sn; pre_dp_e2 = pre_dp_e1 + dp_sn;
        pre_end = dp_end[pre_i]; pre_beg_sn = dp_beg_sn[pre_i]; pre_end_sn = dp_end_sn[pre_i];
        /* set M from (pre_i, q_i-1) */
        if (pre_beg_sn < beg_sn) _beg_sn = beg_sn, first = SIMDShiftRight(pre_dp_h[beg_sn-1], SIMDTotalBytes-SIMDShiftOneNi16);
        else _beg_sn = pre_end_sn, first = SIMDShiftRight(SIMD_MIN_INF, SIMDTotalBytes-SIMDShiftOneNi16);
        _end_sn = MIN_OF_THREE((pre_end+1)/pn, end_sn, dp_sn-1);
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */
            remain = SIMDShiftLeft(pre_dp_h[sn_i], SIMDShiftOneNi16);
            //dp_h[sn_i] = SIMDMaxi16(SIMDOri(first, remain), dp_h[sn_i]);
            SIMDStorei(&dp_h[sn_i], SIMDMaxi16(SIMDOri(first, remain), dp_h[sn_i]));
            first = SIMDShiftRight(pre_dp_h[sn_i], SIMDTotalBytes-SIMDShiftOneNi16);
        }
        /* set E from (pre_i, q_i) */
        _end_sn = MIN_OF_TWO(pre_end_sn, end_sn);
        for (sn_i = _beg_sn; sn_i <= _end_sn; ++sn_i) { /* SIMD parallelization */
            //dp_e1[sn_i] = SIMDMaxi16(pre_dp_e1[sn_i], dp_e1[sn_i]);
            SIMDStorei(&dp_e1[sn_i], SIMDMaxi16(pre_dp_e1[sn_i], dp_e1[sn_i]));
            //dp_e2[sn_i] = SIMDMaxi16(pre_dp_e2[sn_i], dp_e2[sn_i]);
            SIMDStorei(&dp_e2[sn_i], SIMDMaxi16(pre_dp_e2[sn_i], dp_e2[sn_i]));
        }
    }
    /* compare M, E, and F */
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */
        // dp_h[sn_i] = SIMDMaxi16(SIMDAddi16(dp_h[sn_i], q[sn_i]), dp_e1[sn_i]);
        SIMDStorei(&dp_h[sn_i], SIMDMaxi16(SIMDMaxi16(SIMDAddi16(dp_h[sn_i], q[sn_i]), dp_e1[sn_i]), dp_e2[sn_i]));
        //dp_h[sn_i] = SIMDMaxi16(dp_h[sn_i], dp_e2[sn_i]);
        //SIMDStorei(&dp_h[sn_i], SIMDMaxi16(dp_h[sn_i], dp_e2[sn_i]));
    }
    /* new F start */
    first = SIMDShiftRight(SIMDShiftLeft(dp_h[beg_sn], SIMDTotalBytes-SIMDShiftOneNi16), SIMDTotalBytes-SIMDShiftOneNi16); 
    SIMDi first2 = first; int set_num;
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) {
        if (sn_i < min_pre_beg_sn) {
            _err_fatal_simple(__func__, "sn_i < min_pre_beg_sn\n");
        } else if (sn_i > max_pre_end_sn) {
            set_num = sn_i == max_pre_end_sn+1 ? 2 : 1;
        } else set_num = pn;
        /* F = (H << 1 | x) - OE */
        // debug_simd_abpoa_print_cg_matrix_row("before", int16_t, index_i);
        //dp_f[sn_i] = SIMDSubi16(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneNi16), first), GAP_OE1);
        SIMDStorei(&dp_f[sn_i], SIMDSubi16(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneNi16), first), GAP_OE1));
        // dp_f2[sn_i] = SIMDSubi16(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneNi16), first2), GAP_OE2); 
        SIMDStorei(&dp_f2[sn_i], SIMDSubi16(SIMDOri(SIMDShiftLeft(dp_h[sn_i], SIMDShiftOneNi16), first2), GAP_OE2));
        /* F = max{F, (F-e)<<1}, F = max{F, (F-2e)<<2} ... */
        SIMD_SET_F(dp_f[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E1S, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16);
        SIMD_SET_F(dp_f2[sn_i], log_n, set_num, PRE_MIN, PRE_MASK, SUF_MIN, GAP_E2S, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16);
        /* x = max{H, F+o} */
        first = SIMDShiftRight(SIMDMaxi16(dp_h[sn_i], SIMDAddi16(dp_f[sn_i], GAP_O1)), SIMDTotalBytes-SIMDShiftOneNi16);
        first2 = SIMDShiftRight(SIMDMaxi16(dp_h[sn_i], SIMDAddi16(dp_f2[sn_i], GAP_O2)), SIMDTotalBytes-SIMDShiftOneNi16);
        /* H = max{H, F}    */
        //dp_h[sn_i] = SIMDMaxi16(dp_h[sn_i], dp_f[sn_i]);
        SIMDStorei(&dp_h[sn_i], SIMDMaxi16(SIMDMaxi16(dp_h[sn_i], dp_f[sn_i]), dp_f2[sn_i]));
        //dp_h[sn_i] = SIMDMaxi16(dp_h[sn_i], dp_f2[sn_i]);
        //SIMDStorei(&dp_h[sn_i], SIMDMaxi16(dp_h[sn_i], dp_f2[sn_i]));
    }
    // debug_simd_abpoa_print_cg_matrix_row("after", int16_t, index_i);
    for (sn_i = beg_sn; sn_i <= end_sn; ++sn_i) { /* SIMD parallelization */
        /* e for next cell */
        //dp_e1[sn_i] = SIMDMaxi16(SIMDSubi16(dp_e1[sn_i], GAP_E1), SIMDSubi16(dp_h[sn_i], GAP_OE1));
        SIMDStorei(&dp_e1[sn_i], SIMDMaxi16(SIMDSubi16(dp_e1[sn_i], GAP_E1), SIMDSubi16(dp_h[sn_i], GAP_OE1)));
        //dp_e2[sn_i] = SIMDMaxi16(SIMDSubi16(dp_e2[sn_i], GAP_E2), SIMDSubi16(dp_h[sn_i], GAP_OE2));
        SIMDStorei(&dp_e2[sn_i], SIMDMaxi16(SIMDSubi16(dp_e2[sn_i], GAP_E2), SIMDSubi16(dp_h[sn_i], GAP_OE2)));
    }
    /*
    if (abpt->bw >= 0) {                                                                                            
        int max_i = _simd_abpoa_max(SIMD_MIN_INF, zero, inf_min, dp_h, qi, qlen, pn, beg_sn, end_sn);*/
        /*simd_abpoa_max(int16_t, SIMDSetIfGreater, SIMDGetIfGreater);                                            
        int max = inf_min, max_i = -1;                                          
        SIMDi a = dp_h[end_sn], b = qi[end_sn];                             
        if (end_sn == qlen / pn) SIMDSetIfGreateri16(a, zero, b, SIMD_MIN_INF, a);  
        for (i = beg_sn; i < end_sn; ++i) {                                 
            SIMDGetIfGreateri16(b, a, dp_h[i], a, qi[i], b);                   
        }                                                                   
        int16_t *_dp_h = (int16_t*)&a, *_qi = (int16_t*)&b;                           
        for (i = 0; i < pn; ++i) {                                          
            if (_dp_h[i] > max) {                                           
                max = _dp_h[i]; max_i = _qi[i];                             
            }                                                               
        }*/                                                                   

        /*_simd_abpoa_ada_max_i(max_i, graph, node_id);*/
        /* set min/max_rank for next nodes */
        /*int max_out_i = max_i + 1, min_out_i = max_i + 1;
        for (i = 0; i < graph->node[node_id].out_edge_n; ++i) {
            int out_node_id = graph->node[node_id].out_id[i];
            if (max_out_i > graph->node_id_to_max_rank[out_node_id]) graph->node_id_to_max_rank[out_node_id] = max_out_i;
            if (min_out_i < graph->node_id_to_min_rank[out_node_id]) graph->node_id_to_min_rank[out_node_id] = min_out_i;
        } */
    //} 
    return tot_dp_sn;
}

void _simd_abpoa_cg_get_cigar(abpoa_para_t *abpt,abpoa_graph_t *graph, uint8_t *query, abpoa_res_t *res, int best_i, int best_j, int qlen, SIMDi *DP_H2E, int dp_sn, int *pre_n, int **pre_index) {
    int m = abpt->m, n_c = 0, s, m_c = 0, id, hit, cur_op = ABPOA_ALL_OP, _start_i, _start_j;
    int16_t *_pre_dp_h, *_pre_dp_e1, *_pre_dp_e2; abpoa_cigar_t *cigar = 0;
    int i = best_i, j = best_j, k, pre_i; _start_i = best_i, _start_j = best_j;
    id = abpoa_graph_index_to_node_id(graph, i);                                    
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);   
    SIMDi *dp_h = DP_H2E + dp_sn * (i * 3); int16_t *_dp_h = (int16_t*)dp_h; 
    SIMDi *pre_dp_h, *pre_dp_e1, *pre_dp_e2;
    int gap_oe1 = abpt->gap_open1 + abpt->gap_ext1, gap_oe2 = abpt->gap_open2 + abpt->gap_ext2;

    while (i > 0 && j > 0) {                                                                    
        _start_i = i, _start_j = j;
        int *pre_index_i = pre_index[i];                                                                    
        s = abpt->mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            
        printf("s: %d\t %d(%d)\t%d(%d)\n", s, graph->node[id].base, i, query[j-1], j-1);
        if (cur_op & ABPOA_M_OP) {                                                                          
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                /* match/mismatch */                                                                        
                pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (int16_t*)pre_dp_h;                    
                if (_pre_dp_h[j-1] + s == _dp_h[j]) {                                                       
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                  
                    i = pre_i; --j; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                   
                    dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                                
                    cur_op = ABPOA_ALL_OP;                                                                  
                    break;                                                                                  
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0 && cur_op & ABPOA_E_OP) {                                                              
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                /* deletion */                                                                              
                if (cur_op & ABPOA_E1_OP) {                                                                 
                    pre_dp_e1 = DP_H2E + dp_sn * ((pre_i * 3) + 1); _pre_dp_e1 = (int16_t*)pre_dp_e1;       
                    if (_pre_dp_e1[j] == _dp_h[j]) {                                                        
                        pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (int16_t*)pre_dp_h;            
                        if (_pre_dp_h[j] - abpt->gap_ext1 == _pre_dp_e1[j]) cur_op = ABPOA_E1_OP;                 
                        else cur_op = ABPOA_M_OP;                                                           
                        cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                
                        i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                    
                        dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                            
                        break;                                                                              
                    }                                                                                       
                }                                                                                           
                if (cur_op & ABPOA_E2_OP) {                                                                 
                    pre_dp_e2 = DP_H2E + dp_sn * ((pre_i * 3) + 2); _pre_dp_e2 = (int16_t*)pre_dp_e2;       
                    if (_pre_dp_e2[j] == _dp_h[j]) {                                                        
                        pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (int16_t*)pre_dp_h;            
                        if (_pre_dp_h[j] - abpt->gap_ext2 == _pre_dp_e2[j]) cur_op = ABPOA_E_OP;                  
                        else cur_op = ABPOA_M_OP;                                                           
                        cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                
                        i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                    
                        dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                            
                        break;                                                                              
                    }                                                                                       
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0 && cur_op & ABPOA_F_OP) { /* insertion */                                              
            if (cur_op & ABPOA_F1_OP) {                                                                     
                if (_dp_h[j-1] - abpt->gap_ext1 == _dp_h[j]) {
                    hit = 1; cur_op = ABPOA_F1_OP;
                } else if (_dp_h[j-1] - gap_oe1 == _dp_h[j]) {
                    hit = 1; cur_op = ABPOA_M_OP; /*if (_dp_h[j-1] - gap_oe1 == _dp_h[j]) */                        
                }
            } 
            if (hit == 0 && cur_op & ABPOA_F2_OP) { /* F2_OP */                                                                            
                if (_dp_h[j-1] - abpt->gap_ext2 == _dp_h[j]) {
                    hit = 1; cur_op = ABPOA_F_OP; 
                } else if (_dp_h[j-1] - gap_oe2 == _dp_h[j]) {
                    hit = 1; cur_op = ABPOA_M_OP; /*if (_dp_h[j-1] - gap_oe1 == _dp_h[j]) */                        
                }
            }                                                                                               
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); --j;                       
        }                                                                                                   
        // assert(hit == 1);
        printf("%d, %d, %d\n", i, j, cur_op);
    }                                                                                                       
    if (j > 0) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j, -1, j-1);     
    /* reverse cigar */                                                                                     
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                                                         
    res->n_cigar = n_c;                                                                                         
    res->node_s = _start_i, res->node_e = best_i;
    res->query_s = _start_j-1, res->query_e = best_j-1;
}

void _simd_abpoa_cg_backtrack(SIMDi *DP_H2E, int *dp_beg, int *dp_end, int dp_sn, int m, int *mat, int gap_ext1, int gap_ext2, int **pre_index, int *pre_n, int start_i, int start_j, int best_i, int best_j, int qlen, abpoa_graph_t *graph, abpoa_para_t *abpt, uint8_t *query, abpoa_res_t *res) {
    int i, j, k, pre_i, n_c = 0, s, m_c = 0, id, hit, cur_op = ABPOA_ALL_OP, _start_i, _start_j;            
    SIMDi *dp_h, *pre_dp_h, *pre_dp_e1, *pre_dp_e2;                                                        
    int16_t *_dp_h=NULL, *_pre_dp_h, *_pre_dp_e1, *_pre_dp_e2; abpoa_cigar_t *cigar = 0;                    
    i = best_i, j = best_j, _start_i = best_i, _start_j = best_j;                                           
    id = abpoa_graph_index_to_node_id(graph, i);                                                            
    if (best_j < qlen) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, qlen-j, -1, qlen-1);   
    dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                                                
    while (i > start_i && j > start_j) {                                                                    
        _start_i = i, _start_j = j;                                                                         
        int *pre_index_i = pre_index[i];                                                                    
        s = mat[m * graph->node[id].base + query[j-1]]; hit = 0;                                            
        if (cur_op & ABPOA_M_OP) {                                                                          
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                if (j-1 < dp_beg[pre_i] || j-1 > dp_end[pre_i]) continue;                                       \
                pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (int16_t*)pre_dp_h;                    
                if (_pre_dp_h[j-1] + s == _dp_h[j]) { /* match/mismatch */                                  
                    cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CMATCH, 1, id, j-1);                  
                    i = pre_i; --j; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                   
                    dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                                
                    cur_op = ABPOA_ALL_OP;                                                                  
                    ++res->n_aln_bases; res->n_matched_bases += s == mat[0] ? 1 : 0;                        
                    break;                                                                                  
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0 && cur_op & ABPOA_E_OP) {                                                              
            for (k = 0; k < pre_n[i]; ++k) {                                                                
                pre_i = pre_index_i[k];                                                                     
                if (j < dp_beg[pre_i] || j > dp_end[pre_i]) continue;                                       \
                if (cur_op & ABPOA_E1_OP) {                                                                 
                    pre_dp_e1 = DP_H2E + dp_sn * ((pre_i * 3) + 1); _pre_dp_e1 = (int16_t*)pre_dp_e1;       
                    if (_pre_dp_e1[j] == _dp_h[j]) { /* deletion */                                         
                        pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (int16_t*)pre_dp_h;            
                        if (_pre_dp_h[j] - gap_ext1 == _pre_dp_e1[j]) cur_op = ABPOA_E1_OP;                 
                        else cur_op = ABPOA_M_OP;                                                           
                        cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                
                        i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                    
                        dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                            
                        break;                                                                              
                    }                                                                                       
                }                                                                                           
                if (cur_op & ABPOA_E2_OP) {                                                                 
                    pre_dp_e2 = DP_H2E + dp_sn * ((pre_i * 3) + 2); _pre_dp_e2 = (int16_t*)pre_dp_e2;       
                    if (_pre_dp_e2[j] == _dp_h[j]) { /* deletion */                                         
                        pre_dp_h = DP_H2E + dp_sn * (pre_i * 3); _pre_dp_h = (int16_t*)pre_dp_h;            
                        if (_pre_dp_h[j] - gap_ext2 == _pre_dp_e2[j]) cur_op = ABPOA_E_OP;                  
                        else cur_op = ABPOA_M_OP;                                                           
                        cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CDEL, 1, id, j-1);                
                        i = pre_i; id = abpoa_graph_index_to_node_id(graph, i); hit = 1;                    
                        dp_h = DP_H2E + dp_sn * (i * 3); _dp_h = (int16_t*)dp_h;                            
                        break;                                                                              
                    }                                                                                       
                }                                                                                           
            }                                                                                               
        }                                                                                                   
        if (hit == 0) {
            cur_op = ABPOA_M_OP | ABPOA_F_OP;
            cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CINS, 1, id, j-1); --j;                       
            ++res->n_aln_bases;                                                                             
        }
        // printf("%d, %d, %d\n", i, j, cur_op);                                                             
    }                                                                                                       
    if (j > start_j) cigar = abpoa_push_cigar(&n_c, &m_c, cigar, ABPOA_CSOFT_CLIP, j-start_j, -1, j-1);     
    /* reverse cigar */                                                                                     
    res->graph_cigar = abpt->rev_cigar ? cigar : abpoa_reverse_cigar(n_c, cigar);                           
    res->n_cigar = n_c;                                                                                     
    res->node_e = best_i, res->query_e = best_j-1; /* 0-based */                                            
    res->node_s = _start_i, res->query_s = _start_j-1;                                                      
    /*abpoa_print_cigar(n_c, *graph_cigar, graph);*/                                                        
}

int abpoa_cg_global_align_sequence_to_graph_core(abpoa_t *ab, int qlen, uint8_t *query, abpoa_para_t *abpt, SIMD_para_t sp, abpoa_res_t *res) {
    //simd_abpoa_cg_var(int16_t, sp, SIMDSetOne);                                                                         
    //simd_abpoa_var(int16_t, sp, SIMDSetOne);        
    int tot_dp_sn = 0;  
    abpoa_graph_t *graph = ab->abg; abpoa_simd_matrix_t *abm = ab->abm;                             
    int matrix_row_n = graph->node_n, matrix_col_n = qlen + 1;                                      
    int **pre_index, *pre_n;                                                                 
    int i, j, *dp_beg, *dp_beg_sn, *dp_end, *dp_end_sn, node_id, index_i;                   
    int beg_sn, end_sn;
    int pn, log_n, size, qp_sn, dp_sn; /* pn: value per SIMDi, qp_sn/dp_sn/d_sn: segmented length*/              
    SIMDi *dp_h, *dp_f, *qp, *qi=NULL;
    int16_t best_score = sp.inf_min, inf_min = sp.inf_min;               
    int *mat = abpt->mat, best_i = 0, best_j = 0; int16_t gap_ext1 = abpt->gap_ext1;    
    int w = abpt->bw < 0 ? qlen : abpt->bw; /* when w < 0, do whole global */                       
    SIMDi zero = SIMDSetZeroi(), SIMD_MIN_INF = SIMDSetOnei16(inf_min);                                     
    pn = sp.num_of_value; qp_sn = dp_sn = (matrix_col_n + pn - 1) / pn, log_n = sp.log_num, size = sp.size; 
    qp = abm->s_mem;                                                                                

    // simd_abpoa_cg_only_var(int16_t, SIMDSetOne);    
    int16_t gap_open1 = abpt->gap_open1, gap_oe1 = gap_open1 + gap_ext1; 
    int16_t gap_open2 = abpt->gap_open2, gap_ext2 = abpt->gap_ext2, gap_oe2 = gap_open2 + gap_ext2; 
    SIMDi *DP_H2E, *dp_e1, *dp_e2, *dp_f2;
    SIMDi GAP_O1 = SIMDSetOnei16(gap_open1), GAP_O2 = SIMDSetOnei16(gap_open2), GAP_E1 = SIMDSetOnei16(gap_ext1), GAP_E2 = SIMDSetOnei16(gap_ext2), GAP_OE1 = SIMDSetOnei16(gap_oe1), GAP_OE2 = SIMDSetOnei16(gap_oe2);
    DP_H2E = qp + qp_sn * abpt->m; dp_f2 = DP_H2E + dp_sn * matrix_row_n * 3; dp_f = dp_f2 + dp_sn; 

    // for SET_F mask[pn], suf_min[pn], pre_min[logN]
    SIMDi *PRE_MASK, *SUF_MIN, *PRE_MIN, *GAP_E1S, *GAP_E2S;
    PRE_MASK = (SIMDi*)SIMDMalloc((pn+1) * size, size), SUF_MIN = (SIMDi*)SIMDMalloc((pn+1) * size, size), PRE_MIN = (SIMDi*)SIMDMalloc(pn * size, size), GAP_E1S =  (SIMDi*)SIMDMalloc(log_n * size, size), GAP_E2S =  (SIMDi*)SIMDMalloc(log_n * size, size);
    for (i = 0; i < pn; ++i) {
        int16_t *pre_mask = (int16_t*)(PRE_MASK + i);
        for (j = 0; j <= i; ++j) pre_mask[j] = -1;
        for (j = i+1; j < pn; ++j) pre_mask[j] = 0;
    } PRE_MASK[pn] = PRE_MASK[pn-1];
    SUF_MIN[0] = SIMDShiftLeft(SIMD_MIN_INF, SIMDShiftOneNi16); 
    for (i = 1; i < pn; ++i) SUF_MIN[i] = SIMDShiftLeft(SUF_MIN[i-1], SIMDShiftOneNi16); SUF_MIN[pn] = SUF_MIN[pn-1];
    for (i = 1; i < pn; ++i) {
        int16_t *pre_min = (int16_t*)(PRE_MIN + i);
        for (j = 0; j < i; ++j) pre_min[j] = inf_min;
        for (j = i; j < pn; ++j) pre_min[j] = 0;
    }
    GAP_E1S[0] = GAP_E1; GAP_E2S[0] = GAP_E2;
    for (i = 1; i < log_n; ++i) {
        GAP_E1S[i] = SIMDAddi16(GAP_E1S[i-1], GAP_E1S[i-1]); GAP_E2S[i] = SIMDAddi16(GAP_E2S[i-1], GAP_E2S[i-1]);
    }
    _simd_abpoa_init_var(abpt, query, qlen, qp, dp_f, &qi, mat, qp_sn, dp_sn, pn, SIMD_MIN_INF);
    dp_beg = abm->dp_beg, dp_end = abm->dp_end, dp_beg_sn = abm->dp_beg_sn, dp_end_sn = abm->dp_end_sn;
    /* index of pre-node */
    pre_index = (int**)_err_malloc(graph->node_n * sizeof(int*));
    pre_n = (int*)_err_malloc(graph->node_n * sizeof(int*));
    for (i = 0; i < graph->node_n; ++i) {
        node_id = abpoa_graph_index_to_node_id(graph, i); /* i: node index */
        pre_n[i] = graph->node[node_id].in_edge_n;
        pre_index[i] = (int*)_err_malloc(pre_n[i] * sizeof(int));
        for (j = 0; j < pre_n[i]; ++j) {
            pre_index[i][j] = abpoa_graph_node_id_to_index(graph, graph->node[node_id].in_id[j]);
        }
    }
    //simd_abpoa_cg_first_dp();
    //simd_abpoa_cg_first_row;
    _simd_abpoa_cg_first_dp(abpt, graph, dp_beg, dp_end, dp_beg_sn, dp_end_sn, pn, qlen, w, dp_sn, DP_H2E, SIMD_MIN_INF, gap_open1, gap_ext1, gap_open2, gap_ext2, gap_oe1, gap_oe2);

    for (index_i = 1; index_i < matrix_row_n-1; ++index_i) {
        node_id = abpoa_graph_index_to_node_id(graph, index_i); 
        SIMDi *q = qp + graph->node[node_id].base * qp_sn; 
        dp_h = DP_H2E + (index_i*3) * dp_sn; dp_e1 = dp_h + dp_sn; dp_e2 = dp_e1 + dp_sn; 
        tot_dp_sn += _simd_abpoa_cg_dp(q, dp_h, dp_e1, dp_e2, index_i, graph, abpt, dp_sn, pn, qlen, w, DP_H2E, dp_f, dp_f2, SIMD_MIN_INF, GAP_O1, GAP_O2, GAP_E1, GAP_E2, GAP_OE1, GAP_OE2, GAP_E1S, GAP_E2S, PRE_MIN, PRE_MASK, SUF_MIN, log_n, dp_beg, dp_end, dp_beg_sn, dp_end_sn, pre_n, pre_index);
        if (abpt->bw >= 0) {
            beg_sn = dp_beg_sn[index_i], end_sn = dp_end_sn[index_i];
            int max_i = _simd_abpoa_max(SIMD_MIN_INF, zero, inf_min, dp_h, qi, qlen, pn, beg_sn, end_sn);
            _simd_abpoa_ada_max_i(max_i, graph, node_id);
        } 
    }                                                                                                                   
    printf("dp_sn: %d\n", tot_dp_sn);
    // printf("dp_sn: %d, node_n: %d, seq_n: %d\n", tot_dp_sn, graph->node_n, qlen);   
    _simd_abpoa_global_get_max(graph, DP_H2E, 3*dp_sn, qlen,  &best_score, &best_i, &best_j);
    simd_abpoa_print_cg_matrix(int16_t); printf("best_score: (%d, %d) -> %d\n", best_i, best_j, best_score);        
    _simd_abpoa_cg_backtrack(DP_H2E, dp_beg, dp_end, dp_sn, abpt->m, mat, gap_ext1, gap_ext2, pre_index, pre_n, 0, 0, best_i, best_j, qlen, graph, abpt, query, res);
    simd_abpoa_free_var; free(GAP_E1S); free(GAP_E2S);
    return best_score;
}

int simd_abpoa_align_sequence_to_graph(abpoa_t *ab, uint8_t *query, int qlen, abpoa_para_t *abpt, abpoa_res_t *res) {
    if (abpt->simd_flag == 0) err_fatal_simple("No SIMD instruction available.");

    int max_score, bits, mem_ret=0, _best_score=0;
#ifdef __DEBUG__
    _simd_p16.inf_min = MAX_OF_THREE(INT16_MIN + abpt->mismatch, INT16_MIN + abpt->gap_open1 + abpt->gap_ext1, INT16_MIN + abpt->gap_open2 + abpt->gap_ext2);
    if (simd_abpoa_realloc(ab, qlen, abpt, _simd_p16)) return 0;
    if (abpt->gap_mode == ABPOA_CONVEX_GAP) return abpoa_cg_global_align_sequence_to_graph_core(ab, qlen, query, abpt, _simd_p16, res);
    else if (abpt->gap_mode == ABPOA_LINEAR_GAP) return abpoa_lg_global_align_sequence_to_graph_core(ab, qlen, query, abpt, _simd_p16, res);
    else if (abpt->gap_mode == ABPOA_AFFINE_GAP) return abpoa_ag_global_align_sequence_to_graph_core(ab, qlen, query, abpt, _simd_p16, res);
#else
    if (abpt->simd_flag & SIMD_AVX512F && !(abpt->simd_flag & SIMD_AVX512BW)) max_score = INT16_MAX + 1; // AVX512F has no 8/16 bits operations
    else {
        int len = qlen > ab->abg->node_n ? qlen : ab->abg->node_n;
        max_score = MAX_OF_TWO(qlen * abpt->match, len * abpt->gap_ext1 + abpt->gap_open1);
    }
    if (max_score <= INT8_MAX - abpt->mismatch - abpt->gap_open1 - abpt->gap_ext1 - abpt->gap_open2 - abpt->gap_open2) { // DP_H/E/F: 8  bits
        _simd_p8.inf_min = MAX_OF_THREE(INT8_MIN + abpt->mismatch, INT8_MIN + abpt->gap_open1 + abpt->gap_ext1, INT8_MIN + abpt->gap_open2 + abpt->gap_ext2);
        mem_ret = simd_abpoa_realloc(ab, qlen, abpt, _simd_p8);
        bits = 8;
    } else if (max_score <= INT16_MAX - abpt->mismatch - abpt->gap_open1 - abpt->gap_ext1 - abpt->gap_open2 - abpt->gap_open2) { // DP_H/E/F: 16 bits
        _simd_p16.inf_min = MAX_OF_THREE(INT16_MIN + abpt->mismatch, INT16_MIN + abpt->gap_open1 + abpt->gap_ext1, INT16_MIN + abpt->gap_open2 + abpt->gap_ext2);
        mem_ret = simd_abpoa_realloc(ab, qlen, abpt, _simd_p16);
        bits = 16;
    } else { 
        _simd_p32.inf_min = MAX_OF_THREE(INT32_MIN + abpt->mismatch, INT32_MIN + abpt->gap_open1 + abpt->gap_ext1, INT32_MIN + abpt->gap_open2 + abpt->gap_ext2);
        mem_ret = simd_abpoa_realloc(ab, qlen, abpt, _simd_p32);
        bits = 32;
    }
    if (mem_ret) return 0;

    if (abpt->align_mode == ABPOA_GLOBAL_MODE) {
        if (bits == 8) {
            if (abpt->gap_mode == ABPOA_LINEAR_GAP) { // linear gap
                simd_abpoa_lg_global_align_sequence_to_graph_core(int8_t, ab, query, qlen, abpt, res, _simd_p8,  \
                    SIMDSetOnei8, SIMDMaxi8, SIMDAddi8, SIMDSubi8, SIMDShiftOneNi8, SIMDSetIfGreateri8, SIMDGetIfGreateri8,  _best_score);
            } else if (abpt->gap_mode == ABPOA_AFFINE_GAP) { // affine gap
                simd_abpoa_ag_global_align_sequence_to_graph_core(int8_t, ab, query, qlen, abpt, res, _simd_p8,  \
                    SIMDSetOnei8, SIMDMaxi8, SIMDAddi8, SIMDSubi8, SIMDShiftOneNi8, SIMDSetIfGreateri8, SIMDGetIfGreateri8, _best_score);
            } else if (abpt->gap_mode == ABPOA_CONVEX_GAP) { // ABPOA_CONVEX_GAP
                simd_abpoa_cg_global_align_sequence_to_graph_core(int8_t, ab, query, qlen, abpt, res, _simd_p8,  \
                    SIMDSetOnei8, SIMDMaxi8, SIMDAddi8, SIMDSubi8, SIMDShiftOneNi8, SIMDSetIfGreateri8, SIMDGetIfGreateri8,  _best_score);
            }
        } else if (bits == 16) {
            if (abpt->gap_mode == ABPOA_LINEAR_GAP) { // linear gap
                simd_abpoa_lg_global_align_sequence_to_graph_core(int16_t, ab, query, qlen, abpt, res, _simd_p16,    \
                    SIMDSetOnei16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16, SIMDSetIfGreateri16, SIMDGetIfGreateri16, _best_score);
            } else if (abpt->gap_mode == ABPOA_AFFINE_GAP) { // affine gap 
                simd_abpoa_ag_global_align_sequence_to_graph_core(int16_t, ab, query, qlen, abpt, res, _simd_p16,    \
                    SIMDSetOnei16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16, SIMDSetIfGreateri16, SIMDGetIfGreateri16, _best_score);
            } else if (abpt->gap_mode == ABPOA_CONVEX_GAP) { // ABPOA_CONVEX_GAP
                simd_abpoa_cg_global_align_sequence_to_graph_core(int16_t, ab, query, qlen, abpt, res, _simd_p16,    \
                    SIMDSetOnei16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16, SIMDSetIfGreateri16, SIMDGetIfGreateri16, _best_score);
            }
        } else { // 2147483647, DP_H/E/F: 32 bits
            if (abpt->gap_mode == ABPOA_LINEAR_GAP) { // linear gap
                simd_abpoa_lg_global_align_sequence_to_graph_core(int32_t, ab, query, qlen, abpt, res, _simd_p32,    \
                    SIMDSetOnei32, SIMDMaxi32, SIMDAddi32, SIMDSubi32, SIMDShiftOneNi32, SIMDSetIfGreateri32, SIMDGetIfGreateri32, _best_score);
            } else if (abpt->gap_mode == ABPOA_AFFINE_GAP) {
                simd_abpoa_ag_global_align_sequence_to_graph_core(int32_t, ab, query, qlen, abpt, res, _simd_p32,    \
                    SIMDSetOnei32, SIMDMaxi32, SIMDAddi32, SIMDSubi32, SIMDShiftOneNi32, SIMDSetIfGreateri32, SIMDGetIfGreateri32, _best_score);
            } else if (abpt->gap_mode == ABPOA_CONVEX_GAP) {
                simd_abpoa_cg_global_align_sequence_to_graph_core(int32_t, ab, query, qlen, abpt, res, _simd_p32,    \
                    SIMDSetOnei32, SIMDMaxi32, SIMDAddi32, SIMDSubi32, SIMDShiftOneNi32, SIMDSetIfGreateri32, SIMDGetIfGreateri32, _best_score);
            }
        }
    } else if (abpt->align_mode == ABPOA_EXTEND_MODE) {
        if (bits == 8) {
            if (abpt->gap_mode == ABPOA_LINEAR_GAP) {
                simd_abpoa_lg_extend_align_sequence_to_graph_core(int8_t, ab, query, qlen, abpt, res, _simd_p8,  \
                    SIMDSetOnei8, SIMDMaxi8, SIMDAddi8, SIMDSubi8, SIMDShiftOneNi8, SIMDSetIfGreateri8, SIMDGetIfGreateri8, _best_score);
            } else if (abpt->gap_mode == ABPOA_AFFINE_GAP) {
                simd_abpoa_ag_extend_align_sequence_to_graph_core(int8_t, ab, query, qlen, abpt, res, _simd_p8,  \
                    SIMDSetOnei8, SIMDMaxi8, SIMDAddi8, SIMDSubi8, SIMDShiftOneNi8, SIMDSetIfGreateri8, SIMDGetIfGreateri8, _best_score);
            } else if (abpt->gap_mode == ABPOA_CONVEX_GAP) {
                simd_abpoa_cg_extend_align_sequence_to_graph_core(int8_t, ab, query, qlen, abpt, res, _simd_p8,  \
                    SIMDSetOnei8, SIMDMaxi8, SIMDAddi8, SIMDSubi8, SIMDShiftOneNi8, SIMDSetIfGreateri8, SIMDGetIfGreateri8, _best_score);
            }
        } else if (bits == 16) {
            if (abpt->gap_mode == ABPOA_LINEAR_GAP) {
                simd_abpoa_lg_extend_align_sequence_to_graph_core(int16_t, ab, query, qlen, abpt, res, _simd_p16,    \
                    SIMDSetOnei16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16, SIMDSetIfGreateri16, SIMDGetIfGreateri16, _best_score);
            } else if (abpt->gap_mode == ABPOA_AFFINE_GAP) {
                simd_abpoa_ag_extend_align_sequence_to_graph_core(int16_t, ab, query, qlen, abpt, res, _simd_p16,    \
                    SIMDSetOnei16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16, SIMDSetIfGreateri16, SIMDGetIfGreateri16, _best_score);
            } else if (abpt->gap_mode == ABPOA_CONVEX_GAP) {
                simd_abpoa_cg_extend_align_sequence_to_graph_core(int16_t, ab, query, qlen, abpt, res, _simd_p16,    \
                    SIMDSetOnei16, SIMDMaxi16, SIMDAddi16, SIMDSubi16, SIMDShiftOneNi16, SIMDSetIfGreateri16, SIMDGetIfGreateri16, _best_score);
            }
        } else { // 2147483647, DP_H/E/F: 32 bits
            if (abpt->gap_mode == ABPOA_LINEAR_GAP) {
                simd_abpoa_lg_extend_align_sequence_to_graph_core(int32_t, ab, query, qlen, abpt, res, _simd_p32, \
                    SIMDSetOnei32, SIMDMaxi32, SIMDAddi32, SIMDSubi32, SIMDShiftOneNi32, SIMDSetIfGreateri32, SIMDGetIfGreateri32, _best_score);
            } else if (abpt->gap_mode == ABPOA_AFFINE_GAP) {
                simd_abpoa_ag_extend_align_sequence_to_graph_core(int32_t, ab, query, qlen, abpt, res, _simd_p32,    \
                    SIMDSetOnei32, SIMDMaxi32, SIMDAddi32, SIMDSubi32, SIMDShiftOneNi32, SIMDSetIfGreateri32, SIMDGetIfGreateri32, _best_score);
            } else if (abpt->gap_mode == ABPOA_CONVEX_GAP) {
                simd_abpoa_cg_extend_align_sequence_to_graph_core(int32_t, ab, query, qlen, abpt, res, _simd_p32,    \
                    SIMDSetOnei32, SIMDMaxi32, SIMDAddi32, SIMDSubi32, SIMDShiftOneNi32, SIMDSetIfGreateri32, SIMDGetIfGreateri32, _best_score);
            }
        }
    } else err_fatal_core(__func__, "Unknown align mode. (%d).", abpt->align_mode);
#endif
    return _best_score;
}
