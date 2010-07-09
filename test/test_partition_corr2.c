#ifdef P4_TO_P8
#include <p8est_algorithms.h>
#include <p8est_bits.h>
#include <p8est_vtk.h>
#else
#include <p4est_algorithms.h>
#include <p4est_bits.h>
#include <p4est_vtk.h>
#endif


/* typedefs */
typedef struct
{
  p4est_topidx_t a;
}
user_data_t;


/* global variables*/
static int            coarsen_all = 1;


/* functions */
static void
init_fn (p4est_t * p4est, p4est_topidx_t which_tree,
         p4est_quadrant_t * quadrant)
{
  user_data_t        *data = (user_data_t *) quadrant->p.user_data;

  data->a = which_tree;
}

static int
refine_fn (p4est_t * p4est, p4est_topidx_t which_tree,
           p4est_quadrant_t * quadrant)
{
  if (quadrant->level >= 6) {
    return 0;
  }
#ifdef P4_TO_P8
  if (quadrant->level >= 5 && quadrant->z <= P4EST_QUADRANT_LEN (3)) {
    return 0;
  }
#endif

  if (quadrant->x == P4EST_LAST_OFFSET (2) &&
      quadrant->y == P4EST_LAST_OFFSET (2)) {
    return 1;
  }
  if (quadrant->x >= P4EST_QUADRANT_LEN (2)) {
    return 0;
  }

  return 1;
}

static int
coarsen_fn (p4est_t * p4est, p4est_topidx_t which_tree,
            p4est_quadrant_t * q[])
{
  SC_CHECK_ABORT (p4est_quadrant_is_familypv (q), "Coarsen invocation");

  return coarsen_all || q[0]->y >= P4EST_ROOT_LEN / 2;
}


/* main */
int
main (int argc, char **argv)
{
  int                 rank, num_procs, mpiret;
  MPI_Comm            mpicomm = MPI_COMM_WORLD;
  p4est_t            *p4est;
  p4est_connectivity_t *connectivity;


  /* initialize MPI and p4est internals */
  mpiret = MPI_Init (&argc, &argv);
  SC_CHECK_MPI (mpiret);
  mpiret = MPI_Comm_size (mpicomm, &num_procs);
  SC_CHECK_MPI (mpiret);
  mpiret = MPI_Comm_rank (mpicomm, &rank);
  SC_CHECK_MPI (mpiret);

  sc_init (mpicomm, 1, 1, NULL, SC_LP_DEFAULT);
  p4est_init (NULL, SC_LP_DEFAULT);

  /* create connectivity */
#ifdef P4_TO_P8
  connectivity = p8est_connectivity_new_twocubes ();
#else
  connectivity = p4est_connectivity_new_corner ();
#endif

  /* create forest structure */
  p4est = p4est_new (mpicomm, connectivity, 15,
                     sizeof (user_data_t), init_fn, NULL);
  p4est_vtk_write_file (p4est, NULL, P4EST_STRING "_partition_corr_new");

  /* refine */
  p4est_refine (p4est, 1, refine_fn, init_fn);
  p4est_vtk_write_file (p4est, NULL, P4EST_STRING "_partition_corr_refined");

  /* run partition and coarsen till one quadrant per tree remains */
  while (p4est->global_num_quadrants > connectivity->num_trees) {
    p4est_partition (p4est, NULL);
    p4est_coarsen (p4est, 0, coarsen_fn, init_fn);
  }
  SC_CHECK_ABORT (p4est->global_num_quadrants == connectivity->num_trees,
                  "coarsest forest was not achieved");
  p4est_vtk_write_file (p4est, NULL, P4EST_STRING "_partition_corr_coarsened");

  /* destroy the p4est and its connectivity structure */
  p4est_destroy (p4est);
  p4est_connectivity_destroy (connectivity);

  /* clean up and exit */
  sc_finalize ();

  mpiret = MPI_Finalize ();
  SC_CHECK_MPI (mpiret);
  
  return 0;
}
