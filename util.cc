#include "util.h"

timeval g_start_time;
timeval g_end_time;

uint64_t g_memory  = 0;
float    g_runtime = -1.0f;
float    g_ratio   = -1.0f;
float    g_recall  = -1.0f;

// -----------------------------------------------------------------------------
void create_dir(					// create dir if the path exists
	char *path)							// input path
{
	int len = (int) strlen(path);
	for (int i = 0; i < len; ++i) {
		if (path[i] == '/') {
			char ch = path[i + 1];
			path[i + 1] = '\0';
									// check whether the directory exists
			int ret = access(path, F_OK);
			if (ret != 0) {			// create the directory
				ret = mkdir(path, 0755);
				if (ret != 0) {
					printf("Could not create directory %s\n", path);
				}
			}
			path[i + 1] = ch;
		}
	}
}

// -----------------------------------------------------------------------------
int read_txt_data(					// read data (text) from disk
	int   n,							// number of data objects
	int   d,							// dimensionality
	const char *fname,					// address of data set
	float **data,						// data objects (return)
	float **norm_d)						// l2-norm of data objects (return)
{
	gettimeofday(&g_start_time, NULL);
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		printf("Could not open %s\n", fname);
		return 1;
	}

	int i = 0;
	int j = 0;
	while (!feof(fp) && i < n) {
		float tmp = 0.0f;
		memset(norm_d[i], 0.0f, NORM_K * SIZEFLOAT);

		fscanf(fp, "%d", &j);
		for (j = 0; j < d; ++j) {
			fscanf(fp, " %f", &tmp);
			data[i][j] = tmp;

			norm_d[i][0] += tmp*tmp;
			for (int t = 1; t < NORM_K; ++t) {
				if (j < 8*t) norm_d[i][t] += tmp*tmp;
			}
		}
		fscanf(fp, "\n");

		for (int t = 1; t < NORM_K; ++t) {
			norm_d[i][t] = sqrt(norm_d[i][0] - norm_d[i][t]);
		}
		norm_d[i][0] = sqrt(norm_d[i][0]);
		++i;
	}
	assert(feof(fp) && i == n);
	fclose(fp);

	gettimeofday(&g_end_time, NULL);
	float running_time = g_end_time.tv_sec - g_start_time.tv_sec + 
		(g_end_time.tv_usec - g_start_time.tv_usec) / 1000000.0f;
	printf("Read Data: %f Seconds\n\n", running_time);

	return 0;
}

// -----------------------------------------------------------------------------
int read_bin_data(					// read data (binary) from disk
	int   n,							// number of data objects
	int   d,							// dimensionality
	const char *fname,					// address of data
	float **data,						// data objects (return)
	float **norm_d)						// l2-norm of data objects (return)
{
	gettimeofday(&g_start_time, NULL);
	FILE *fp = fopen(fname, "rb");
	if (!fp) {
		printf("Could not open %s\n", fname);
		return 1;
	}

	int i = 0;
	while (!feof(fp) && i < n) {
		fread(data[i], SIZEFLOAT, d, fp);

		// calc norm_d
		memset(norm_d[i], 0.0f, NORM_K * SIZEFLOAT);
		float tmp = 0.0f;
		for (int j = 0; j < d; ++j) {
			tmp = data[i][j];

			norm_d[i][0] += SQR(tmp);
			for (int t = 1; t < NORM_K; ++t) {
				if (j < 8 * t) norm_d[i][t] += SQR(tmp);
			}
		}
		for (int t = 1; t < NORM_K; ++t) {
			norm_d[i][t] = sqrt(norm_d[i][0] - norm_d[i][t]);
		}
		norm_d[i][0] = sqrt(norm_d[i][0]);
		++i;
	}
	fclose(fp);

	gettimeofday(&g_end_time, NULL);
	float running_time = g_end_time.tv_sec - g_start_time.tv_sec + 
		(g_end_time.tv_usec - g_start_time.tv_usec) / 1000000.0f;
	printf("Read Data: %f Seconds\n\n", running_time);

	return 0;
}

// -----------------------------------------------------------------------------
int read_ground_truth(				// read ground truth results from disk
	int qn,								// number of query objects
	const char *fname,					// address of truth set
	Result **R)							// ground truth results (return)
{
	gettimeofday(&g_start_time, NULL);
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		printf("Could not open %s\n", fname);
		return 1;
	}

	int tmp1 = -1;
	int tmp2 = -1;
	fscanf(fp, "%d %d\n", &tmp1, &tmp2);
	assert(tmp1 == qn && tmp2 == MAXK);

	for (int i = 0; i < qn; ++i) {
		for (int j = 0; j < MAXK; ++j) {
			fscanf(fp, "%d %f ", &R[i][j].id_, &R[i][j].key_);
		}
		fscanf(fp, "\n");
	}
	fclose(fp);

	gettimeofday(&g_end_time, NULL);
	float running_time = g_end_time.tv_sec - g_start_time.tv_sec + 
		(g_end_time.tv_usec - g_start_time.tv_usec) / 1000000.0f;
	printf("Read Ground Truth: %f Seconds\n\n", running_time);

	return 0;
}

// -----------------------------------------------------------------------------
float calc_inner_product(			// calc inner product
	int   dim,							// dimension
	const float *p1,					// 1st point
	const float *p2)					// 2nd point
{
	float ret = 0.0f;
	for (int i = 0; i < dim; ++i) {
		ret += p1[i] * p2[i];
	}
	return ret;
}

// -----------------------------------------------------------------------------
float calc_inner_product(			// calc inner product
	int   dim,							// dimension
	float threshold,					// threshold
	const float *p1,					// 1st point
	const float *norm1,					// l2-norm of 1st point
	const float *p2,					// 2nd point
	const float *norm2) 				// l2-norm of 2nd point
{
	float ip = 0.0f;
	int base = 0;
	for (int t = 1; t < NORM_K; ++t) {
		int end = base + 8;
		for (int i = base; i < end; ++i) {
			ip += p1[i] * p2[i];
		}
		if (ip + norm1[t]*norm2[t] <= threshold) return ip; 
		base += 8;
	}
	for (int i = base; i < dim; ++i) {
		ip += p1[i] * p2[i];
	}
	return ip;

	// unsigned d = dim & ~unsigned(7);
	// const float *aa = p1, *end_a = aa + d;
	// const float *bb = p2, *end_b = bb + d;

	// float r = 0.0f;
	// float r0, r1, r2, r3, r4, r5, r6, r7;

	// const float *a = aa, *b = bb;
	// int t = 1;
	// for (; a < end_a; a += 8, b += 8) {
	// 	__builtin_prefetch(a+32, 0, 3);
	// 	__builtin_prefetch(b+32, 0, 0);

	// 	r0 = a[0] * b[0];
	// 	r1 = a[1] * b[1];
	// 	r2 = a[2] * b[2];
	// 	r3 = a[3] * b[3];
	// 	r4 = a[4] * b[4];
	// 	r5 = a[5] * b[5];
	// 	r6 = a[6] * b[6];
	// 	r7 = a[7] * b[7];

	// 	r += r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7;

	// 	if (t < NORM_K && r+norm1[t]*norm2[t] <= threshold) return r;
	// 	++t;
	// }
	// __builtin_prefetch(a, 0, 3);
	// __builtin_prefetch(b, 0, 0);

	// r0 = r1 = r2 = r3 = r4 = r5 = r6 = r7 = 0.0f;
	// switch (dim & 7) {
	// 	case 7: r6 = a[6] * b[6];
	// 	case 6: r5 = a[5] * b[5];
	// 	case 5: r4 = a[4] * b[4];
	// 	case 4: r3 = a[3] * b[3];
	// 	case 3: r2 = a[2] * b[2];
	// 	case 2: r1 = a[1] * b[1];
	// 	case 1: r0 = a[0] * b[0];
	// }
	// r += r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7;
	
	// return r;
}

// -----------------------------------------------------------------------------
float calc_l2_sqr(					// calc L2 square distance
	int   dim,							// dimension
	float threshold,					// threshold
	const float *p1,					// 1st point
	const float *p2)					// 2nd point
{
	unsigned d = dim & ~unsigned(7);
	const float *aa = p1, *end_a = aa + d;
	const float *bb = p2, *end_b = bb + d;

	__builtin_prefetch(aa, 0, 3);
	__builtin_prefetch(bb, 0, 0);

	float r = 0.0f;
	float r0, r1, r2, r3, r4, r5, r6, r7;

	const float *a = end_a, *b = end_b;

	r0 = r1 = r2 = r3 = r4 = r5 = r6 = r7 = 0.0f;
	switch (dim & 7) {
		case 7: r6 = SQR(a[6] - b[6]);
		case 6: r5 = SQR(a[5] - b[5]);
		case 5: r4 = SQR(a[4] - b[4]);
		case 4: r3 = SQR(a[3] - b[3]);
		case 3: r2 = SQR(a[2] - b[2]);
		case 2: r1 = SQR(a[1] - b[1]);
		case 1: r0 = SQR(a[0] - b[0]);
	}

	a = aa; b = bb;
	for (; a < end_a; a += 8, b += 8) {
		__builtin_prefetch(a+32, 0, 3);
		__builtin_prefetch(b+32, 0, 0);

		r += (r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7);
		if (r > threshold) return r;

		r0 = SQR(a[0] - b[0]);
		r1 = SQR(a[1] - b[1]);
		r2 = SQR(a[2] - b[2]);
		r3 = SQR(a[3] - b[3]);
		r4 = SQR(a[4] - b[4]);
		r5 = SQR(a[5] - b[5]);
		r6 = SQR(a[6] - b[6]);
		r7 = SQR(a[7] - b[7]);
	}
	r += (r0 + r1 + r2 + r3 + r4 + r5 + r6 + r7);
	
	return r;
}

// -----------------------------------------------------------------------------
float calc_recall(					// calc recall of mip results
	int   k,							// top-k value
	const Result *R,					// ground truth results 
	MaxK_List *list)					// results returned by algorithms
{
	int i = k - 1;
	int last = k - 1;
	while (i >= 0 && R[last].key_ - list->ith_key(i) > FLOATZERO) --i;
	
	return (i + 1) * 100.0f / k;
}

// -----------------------------------------------------------------------------
float calc_recall(					// calc recall of mip results
	int   k,							// top-k value
	const Result *R,					// ground truth results 
	const Result *result)				// MIP results
{
	int i = k - 1;
	int last = k - 1;
	while (i >= 0 && R[last].key_ - result[i].key_ > FLOATZERO) --i;

	return (i + 1) * 100.0f / k;
}

// -----------------------------------------------------------------------------
int get_hits(						// get the number of hits between two ID list
	int   k,							// top-k value
	int   t,							// top-t value
	const Result *R,					// ground truth results 
	MaxK_List *list)					// results returned by algorithms
{
	int i = k - 1;
	int last = t - 1;
	while (i >= 0 && R[last].key_ - list->ith_key(i) > FLOATZERO) --i;

	return MIN(t, i + 1);
}

// -----------------------------------------------------------------------------
int norm_distribution(				// analyse norm distribution of data
	int   n,							// number of data objects
	int   d,							// dimensionality
	const float **data,					// data objects
	const float **norm_d,				// l2-norm of data objects
	const char  *out_path)				// output path
{
	// -------------------------------------------------------------------------
	//  find max l2-norm of all data objects
	// -------------------------------------------------------------------------
	gettimeofday(&g_start_time, NULL);
	float max_norm = MINREAL;
	for (int i = 0; i < n; ++i) {
		if (norm_d[i][0] > max_norm) max_norm = norm_d[i][0];
	}

	// -------------------------------------------------------------------------
	//  get the percentage of frequency of norm
	// -------------------------------------------------------------------------
	int m = 25;
	float interval = max_norm / m;
	printf("m = %d, max_norm = %f, interval = %f\n", m, max_norm, interval);

	std::vector<int> freq(m, 0);
	for (int i = 0; i < n; ++i) {
		int id = (int) ceil(norm_d[i][0] / interval) - 1;
		if (id < 0)  id = 0;
		if (id >= m) id = m - 1;
		freq[id]++;
	}

	// -------------------------------------------------------------------------
	//  write norm distribution
	// -------------------------------------------------------------------------
	char output_set[200];
	sprintf(output_set, "%snorm_distribution.out", out_path);

	FILE *fp = fopen(output_set, "w");
	if (!fp) {
		printf("Could not create %s\n", output_set);
		return 1;
	}

	float num  = 0.5f / m;
	float step = 1.0f / m;
	for (int i = 0; i < m; ++i) {
		fprintf(fp, "%.1f\t%f\n", (num+step*i)*100.0f, freq[i]*100.0f/n);
	}
	fprintf(fp, "\n");
	fclose(fp);

	gettimeofday(&g_end_time, NULL);
	float runtime = g_end_time.tv_sec - g_start_time.tv_sec + 
		(g_end_time.tv_usec - g_start_time.tv_usec) / 1000000.0f;
	printf("Norm distribution: %.6f Seconds\n\n", runtime);

	return 0;
}

// -----------------------------------------------------------------------------
void k_mip_search(					// k-MIP search
	int   n, 							// number of data objects
	int   qn,							// number of query objects
	int   d, 							// dimensionality
	int   k,							// top-k value
	const float **data,					// data objects
	const float **norm_d,				// l2-norm of data objects
	const float **query,				// query objects
	const float **norm_q,				// l2-norm of query objects
	Result **result)					// k-MIP results (return)
{
	// -------------------------------------------------------------------------
	//  calc the norm of data and query
	// -------------------------------------------------------------------------
	Result *order_d = new Result[n];
	for (int i = 0; i < n; ++i) {
		order_d[i].id_  = i;
		order_d[i].key_ = norm_d[i][0];
	}
	qsort(order_d, n, sizeof(Result), ResultCompDesc);

	// -------------------------------------------------------------------------
	//  k-MIP search by linear scan with pruning
	// -------------------------------------------------------------------------
	MaxK_List *list = new MaxK_List(k);
	for (int i = 0; i < qn; ++i) {
		float kip = MINREAL;

		list->reset();
		for (int j = 0; j < n; ++j) {
 			int id = order_d[j].id_;
			if (norm_d[id][0] * norm_q[i][0] <= kip) break;

			float ip = calc_inner_product(d, kip, data[id], norm_d[id], 
				query[i], norm_q[i]);
			kip = list->insert(ip, id + 1);
		}

		for (int j = 0; j < k; ++j) {
			result[i][j].id_  = list->ith_id(j);
			result[i][j].key_ = list->ith_key(j);
		}
	}
	delete list; list = NULL;
	delete[] order_d; order_d = NULL;
}

// -----------------------------------------------------------------------------
int ground_truth(					// find the ground truth MIP results
	int   n,							// number of data objects
	int   qn,							// number of query points
	int   d,							// dimensionality
	const float **data,					// data objects
	const float **norm_d,				// l2-norm of data objects
	const float **query,				// query objects
	const float **norm_q,				// l2-norm of query objects
	const char  *truth_set) 			// address of truth set
{
	gettimeofday(&g_start_time, NULL);
	FILE *fp = fopen(truth_set, "w");
	if (!fp) {
		printf("Could not create %s\n", truth_set);
		return 1;
	}

	// -------------------------------------------------------------------------
	//  find ground truth results (using linear scan method)
	// -------------------------------------------------------------------------
	Result **result = new Result*[qn];
	for (int i = 0; i < qn; ++i) {
		result[i] = new Result[MAXK];
	}
	k_mip_search(n, qn, d, MAXK, data, norm_d, query, norm_q, result);

	fprintf(fp, "%d %d\n", qn, MAXK);
	for (int i = 0; i < qn; ++i) {
		for (int j = 0; j < MAXK; ++j) {
			fprintf(fp, "%d %f ", result[i][j].id_, result[i][j].key_);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);

	// -------------------------------------------------------------------------
	//  release space
	// -------------------------------------------------------------------------
	for (int i = 0; i < qn; ++i) {
		delete[] result[i]; result[i] = NULL;
	}
	delete[] result; result = NULL;

	gettimeofday(&g_end_time, NULL);
	float truth_time = g_end_time.tv_sec - g_start_time.tv_sec + 
		(g_end_time.tv_usec - g_start_time.tv_usec) / 1000000.0f;
	printf("Ground Truth: %f Seconds\n\n", truth_time);
	
	return 0;
}

