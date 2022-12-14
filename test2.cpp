// ALL CAPS VARIABLES ARE DATA ARRAYS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <thread>

#if defined (WIN32) || (_WIN64)

#include <windows.h>
#define pthread_t DWORD
#define pthread_create(THREAD_ID_PTR, ATTR, ROUTINE, PARAMS) CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ROUTINE,(void*)PARAMS,0,THREAD_ID_PTR)
#define sleep(ms) Sleep(ms)

#else // Linux

#include <pthread.h>
#include <unistd.h>

#endif

int MAX_THREADS = std::thread::hardware_concurrency();
int thread_count = 1;


// a basic binary search
int binary_search(double val, double* array, int p, int r)
{
	int high = p > (r + 1) ? p : (r + 1);
	while (p < high)
	{
		int mid = (p + high) / 2;
		if (val <= array[mid])
			high = mid;
		else
			p = mid + 1;
	}
	return high;
}


void swap(int& a, int& b)
{
	int tmp = a;
	a = b;
	b = tmp;
}


class p_merge_args
{
public:
	double* TMP, * A;
	int p1, r1, p2, r2, p3, busy;
	p_merge_args(double* TMP, int p1, int r1, int p2, int r2, double* A, int p3, int thread) : TMP(TMP), p1(p1), r1(r1), p2(p2), r2(r2), A(A), p3(p3), busy(thread) {}
	~p_merge_args() { TMP = nullptr; }
};

// parallel merge function
void p_merge(p_merge_args *args)
{
	int n1 = args->r1 - args->p1 + 1;
	int n2 = args->r2 - args->p2 + 1;

	if (n1 < n2)
	{
		swap(args->p1, args->p2);
		swap(args->r1, args->r2);
		swap(n1, n2);
	}
	if (n1 != 0)
	{
		int q1 = (args->p1 + args->r1) / 2;
		int q2 = binary_search(args->TMP[q1], args->TMP, args->p2, args->r2);
		int q3 = args->p3 + (q1 - args->p1) + (q2 - args->p2);

		args->A[q3] = args->TMP[q1];

		p_merge_args* args1 = new p_merge_args(args->TMP, args->p1, q1 - 1, args->p2, q2 - 1, args->A, args->p3, false);
		if (thread_count >= MAX_THREADS)
			p_merge(args1);
		else
		{
			args1->busy = ++thread_count;
			pthread_create(0, 0, p_merge, args1); // spawn
		}

		p_merge_args args2(args->TMP, q1 + 1, args->r1, q2, args->r2, args->A, q3 + 1, false);
		p_merge(&args2);

		while (args1->busy) // sync
			sleep(5);
		delete args1;
	}
	if (args->busy)
	{
		args->busy = 0;
		thread_count--;
	}
}


class p_merge_sort_args
{
public:
	double* A, * B;
	int p, r, s, busy;
	p_merge_sort_args(double* A, int p, int r, double* B, int s, int thread) : A(A), p(p), r(r), B(B), s(s), busy(thread) {}
	~p_merge_sort_args() { A = B = nullptr; }
};

// parallel merge sort function
void p_merge_sort(p_merge_sort_args *args)
{
	int n = args->r - args->p + 1;

	if (n == 1)
		args->B[args->s] = args->A[args->p];
	else
	{
		double* TMP = new double[n + 1];
		int q1 = (args->p + args->r) / 2;
		int q2 = q1 - args->p + 1;

		p_merge_sort_args* args1 = new p_merge_sort_args(args->A, args->p, q1, TMP, 1, false);
		if(thread_count >= MAX_THREADS)
			p_merge_sort(args1);
		else
		{
			args1->busy = ++thread_count;
			pthread_create(0, 0, p_merge_sort, args1); // spawn
		}

		p_merge_sort_args args2(args->A, q1 + 1, args->r, TMP, q2 + 1, false);
		p_merge_sort(&args2);

		while (args1->busy) // sync
			sleep(5);
		delete args1;

		p_merge_args* merge_args = new p_merge_args(TMP, 1, q2, q2 + 1, n, args->B, args->s, false);
		p_merge(merge_args);

		delete[]TMP;
	}
	if (args->busy)
	{
		args->busy = 0;
		thread_count--;
	}
}

// driver
int main(int argc, char** argv)
{
	char* sz;

	int MAX_ARRAY_ELEMENTS = 1000;

	// parse command line arguments
	for (--argc, ++argv; argc > 0; --argc, ++argv)
	{
		sz = *argv;
		if (*sz != '-')
			break;

		switch (sz[1])
		{
		case 'A':  // array max size
			MAX_ARRAY_ELEMENTS = atoi(sz + 2);
			break;

		case 'T':  // maximum thread count
			MAX_THREADS = atoi(sz + 2);
			break;
		}
	}

	printf("\n\nArray[%d]", MAX_ARRAY_ELEMENTS);

	// allocate the arrays
	double* A = new double[MAX_ARRAY_ELEMENTS];
	double* B = new double[MAX_ARRAY_ELEMENTS];

	// generating random values in array to be sorted
	srand(clock());
	for (int i = 0; i < MAX_ARRAY_ELEMENTS; i++)
		A[i] = (double)rand();

	printf("\n\nArray Randomized");

	clock_t time = clock();

	p_merge_sort_args args(A, 0, MAX_ARRAY_ELEMENTS - 1, B, 0, false); // A (source array), start index, end index, B (destination array), start index, thread ID (or false)
	p_merge_sort(&args);

	printf("\n\nSorted in %Lf Seconds", (clock() - time) / 1000.0L);

	double last = 0.0;
	for (int i = 0; i < MAX_ARRAY_ELEMENTS; i++)
	{
		if (B[i] < last)
		{
			printf("\n\nArray Not Sorted");
			return 0;
		}
		last = B[i];
	}

	printf("\n\nArray Sorted");
	if (MAX_ARRAY_ELEMENTS < 50)
		for (int i = 0; i < MAX_ARRAY_ELEMENTS; i++)
			printf(" %f", B[i]);
	printf("\n");

	delete[]B;
	delete[]A;

	return 0;
}