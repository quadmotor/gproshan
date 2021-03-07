#include "geodesics/heat_method.h"

#include "util.h"
#include "laplacian/laplacian.h"

#include <cassert>

using namespace std;


// geometry processing and shape analysis framework
namespace gproshan {


double heat_method(real_t * dist, const che * mesh, const std::vector<index_t> & sources, const heat_method_opt & opt)
{
	if(!sources.size()) return 0;

	// build impulse signal
	a_vec u0(mesh->n_vertices, arma::fill::zeros);
	for(auto & v: sources) u0(v) = 1;

	// step
	real_t dt = mesh->mean_edge();
	dt *= dt;

	a_sp_mat L, A;
	laplacian(mesh, L, A);

	// make L positive-definite
	L += 1.0e-8 * A;

	// heat flow for short interval
	A += dt * L;
	a_vec u(mesh->n_vertices);

	cholmod_common context;
	cholmod_l_start(&context);

	double solve_time = 0;

	switch(opt)
	{
		case HEAT_ARMA:
			if(!spsolve(u, A, u0)) gproshan_error(arma: no solution);
			break;
		case HEAT_CHOLMOD:
			solve_time += solve_positive_definite(u, A, u0, &context);
			break;
	#ifdef GPROSHAN_CUDA
		case HEAT_CUDA:
			solve_time += solve_positive_definite_gpu(u, A, u0);
			break;
	#endif // GPROSHAN_CUDA
	}

	// extract geodesics

	a_vec div(mesh->n_vertices);
	compute_divergence(mesh, u, div);

	a_vec phi(dist, mesh->n_vertices, false);

	switch(opt)
	{
		case HEAT_ARMA:
			if(!spsolve(phi, L, div)) gproshan_error(arma: no solution);
			break;
		case HEAT_CHOLMOD:
			solve_time += solve_positive_definite(phi, L, div, &context);
			break;
	#ifdef GPROSHAN_CUDA
		case HEAT_CUDA:
			solve_time += solve_positive_definite_gpu(phi, L, div);
			break;
	#endif // GPROSHAN_CUDA
	}

	real_t min_val = phi.min();
	phi -= min_val;
	phi *= 0.5;

	cholmod_l_finish(&context);

	return solve_time;
}

void compute_divergence(const che * mesh, const a_mat & u, a_mat & div)
{
	for(index_t v = 0; v < mesh->n_vertices; ++v)
	{
		real_t & sum = div(v);

		sum = 0;
		for_star(he, mesh, v)
			sum += (
					mesh->normal_he(he) * ( mesh->gt_vt(prev(he)) - mesh->gt_vt(next(he)) ) ,
					- mesh->gradient_he(he, u.memptr())
					);
	}
}

double solve_positive_definite(a_mat & x, const a_sp_mat & A, const a_mat & b, cholmod_common * context)
{
	cholmod_sparse * cA = arma_2_cholmod(A, context);
	cA->stype = 1;

	cholmod_dense * cb = arma_2_cholmod(b, context);

	cholmod_factor * L = cholmod_l_analyze(cA, context);
	cholmod_l_factorize(cA, L, context);

	/* fill ratio
	gproshan_debug_var(L->xsize);
	gproshan_debug_var(cA->nzmax);
	gproshan_debug_var(L->xsize / cA->nzmax);
	*/

	double solve_time;
	TIC(solve_time)
	cholmod_dense * cx = cholmod_l_solve(CHOLMOD_A, L, cb, context);
	TOC(solve_time)

	assert(x.n_rows == b.n_rows);
	copy_real_t_array(x.memptr(), (double *) cx->x, x.n_rows);

	cholmod_l_free_factor(&L, context);
	cholmod_l_free_sparse(&cA, context);
	cholmod_l_free_dense(&cb, context);

	return solve_time;
}

cholmod_dense * arma_2_cholmod(const a_mat & D, cholmod_common * context)
{
	cholmod_dense * cD = cholmod_l_allocate_dense(D.n_rows, D.n_cols, D.n_rows, CHOLMOD_REAL, context);
	copy_real_t_array((double *) cD->x, D.memptr(), D.n_elem);

	return cD;
}

cholmod_sparse * arma_2_cholmod(const a_sp_mat & S, cholmod_common * context)
{
	assert(sizeof(arma::uword) == sizeof(SuiteSparse_long));

	cholmod_sparse * cS = cholmod_l_allocate_sparse(S.n_rows, S.n_cols, S.n_nonzero, 1, 1, 0, CHOLMOD_REAL, context);

	copy_real_t_array((double *) cS->x, S.values, S.n_nonzero);
	memcpy(cS->p, S.col_ptrs, (S.n_cols + 1) * sizeof(arma::uword));
	memcpy(cS->i, S.row_indices, S.n_nonzero * sizeof(arma::uword));

	return cS;
}


#ifdef GPROSHAN_CUDA

double solve_positive_definite_gpu(a_mat & x, const a_sp_mat & A, const a_mat & b)
{
	int * hA_col_ptrs = new int[A.n_cols + 1];
	int * hA_row_indices = new int[A.n_nonzero];

	#pragma omp parallel for
	for(index_t i = 0; i <= A.n_cols; ++i)
		hA_col_ptrs[i] = A.col_ptrs[i];

	#pragma omp parallel for
	for(index_t i = 0; i < A.n_nonzero; ++i)
		hA_row_indices[i] = A.row_indices[i];

	double solve_time = solve_positive_definite_cusolver(A.n_rows, A.n_nonzero, A.values, hA_col_ptrs, hA_row_indices, b.memptr(), x.memptr());

	delete [] hA_col_ptrs;
	delete [] hA_row_indices;

	return solve_time;
}

#endif // GPROSHAN_CUDA


} // namespace gproshan
