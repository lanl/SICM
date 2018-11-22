
#ifndef WIN32
#include <unistd.h>
#endif

#include <iomanip>

#include <algorithm>
#include <cstdio>
#include <vector>
#include <iostream>
#include <string>
#include <ctime>

#include <AMReX_Utility.H>
#include <AMReX_CONSTANTS.H>
#include <Castro.H>
#include <Castro_F.H>
#include <AMReX_VisMF.H>
#include <AMReX_TagBox.H>
#include <AMReX_FillPatchUtil.H>
#include <AMReX_ParmParse.H>
#include <Castro_error_F.H>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace amrex;

int          Castro::verbose       = 0;
Real         Castro::fixed_dt      = -1.0;
int          Castro::sum_interval  = -1;
ErrorList    Castro::err_list;
int          Castro::radius_grow   = 1;
BCRec        Castro::phys_bc;
int          Castro::NUM_STATE     = -1;
int          Castro::NUM_GROW      = -1;

long         Castro::num_zones_advanced = 0;

Real         Castro::frac_change   = 1.e200;

int          Castro::Density       = -1;
int          Castro::Eden          = -1;
int          Castro::Eint          = -1;
int          Castro::Temp          = -1;
int          Castro::Xmom          = -1;
int          Castro::Ymom          = -1;
int          Castro::Zmom          = -1;
int          Castro::NumSpec       = 0;
int          Castro::FirstSpec     = -1;

int          Castro::NumAux        = 0;
int          Castro::FirstAux      = -1;

int          Castro::NumAdv        = 0;
int          Castro::FirstAdv      = -1;

int          Castro::QVAR          = -1;
int          Castro::QRADVAR       = 0;
int          Castro::NQAUX         = -1;
int          Castro::NQ            = -1;
int          Castro::NGDNV         = -1;

int          Castro::MOL_STAGES;
Vector< Vector<Real> > Castro::a_mol;
Vector<Real> Castro::b_mol;
Vector<Real> Castro::c_mol;


#include <castro_defaults.H>

std::string  Castro::probin_file = "probin";

#ifdef AMREX_USE_GPU
IntVect      Castro::hydro_tile_size(1024000,1024000,1024000);
#else
IntVect      Castro::hydro_tile_size(1024,16,16);
#endif

int          Castro::num_state_type = 0;

// Note: Castro::variableSetUp is in Castro_setup.cpp

void
Castro::variableCleanUp ()
{
    desc_lst.clear();

    eos_finalize();

    network_finalize();

    ca_extern_finalize();

    ca_destroy_grid_info();

    ca_destroy_method_params();

    ca_destroy_castro_method_params();

    ca_destroy_problem_params();

    probinit_finalize();
}

void
Castro::read_params ()
{
    static bool done = false;

    if (done) return;

    done = true;

    ParmParse pp("castro");

#include <castro_queries.H>

    pp.query("v",verbose);
    pp.query("fixed_dt",fixed_dt);
    pp.query("sum_interval",sum_interval);

    // Get boundary conditions
    Vector<int> lo_bc(BL_SPACEDIM), hi_bc(BL_SPACEDIM);
    pp.getarr("lo_bc",lo_bc,0,BL_SPACEDIM);
    pp.getarr("hi_bc",hi_bc,0,BL_SPACEDIM);
    for (int i = 0; i < BL_SPACEDIM; i++)
    {
        phys_bc.setLo(i,lo_bc[i]);
        phys_bc.setHi(i,hi_bc[i]);
    }

    //
    // Check phys_bc against possible periodic geometry
    // if periodic, must have internal BC marked.
    //
    if (Geometry::isAnyPeriodic())
    {
        //
        // Do idiot check.  Periodic means interior in those directions.
        //
        for (int dir = 0; dir<BL_SPACEDIM; dir++)
        {
            if (Geometry::isPeriodic(dir))
            {
                if (lo_bc[dir] != Interior)
                {
                    std::cerr << "Castro::read_params:periodic in direction "
                              << dir
                              << " but low BC is not Interior\n";
                    amrex::Error();
                }
                if (hi_bc[dir] != Interior)
                {
                    std::cerr << "Castro::read_params:periodic in direction "
                              << dir
                              << " but high BC is not Interior\n";
                    amrex::Error();
                }
            }
        }
    }
    else
    {
        //
        // Do idiot check.  If not periodic, should be no interior.
        //
        for (int dir=0; dir<BL_SPACEDIM; dir++)
        {
            if (lo_bc[dir] == Interior)
            {
                std::cerr << "Castro::read_params:interior bc in direction "
                          << dir
                          << " but not periodic\n";
                amrex::Error();
            }
            if (hi_bc[dir] == Interior)
            {
                std::cerr << "Castro::read_params:interior bc in direction "
                          << dir
                          << " but not periodic\n";
                amrex::Error();
            }
        }
    }

    if ( Geometry::IsRZ() && (lo_bc[0] != Symmetry) ) {
        std::cerr << "ERROR:Castro::read_params: must set r=0 boundary condition to Symmetry for r-z\n";
        amrex::Error();
    }

    if ( Geometry::IsRZ() )
    {
	amrex::Abort("We don't support cylindrical coordinate systems in 3D");
    }
    else if ( Geometry::IsSPHERICAL() )
    {
	amrex::Abort("We don't support spherical coordinate systems in 3D");
    }

    // sanity checks

    if (cfl <= 0.0 || cfl > 1.0)
      amrex::Error("Invalid CFL factor; must be between zero and one.");

    int bndry_func_thread_safe = 1;
    StateDescriptor::setBndryFuncThreadSafety(bndry_func_thread_safe);

    ParmParse ppa("amr");
    ppa.query("probin_file",probin_file);

    Vector<int> tilesize(BL_SPACEDIM);
    if (pp.queryarr("hydro_tile_size", tilesize, 0, BL_SPACEDIM))
    {
	for (int i=0; i<BL_SPACEDIM; i++) hydro_tile_size[i] = tilesize[i];
    }

}

Castro::Castro ()
{
}

Castro::Castro (Amr&            papa,
                int             lev,
                const Geometry& level_geom,
                const BoxArray& bl,
		const DistributionMapping& dm,
                Real            time)
    :
    AmrLevel(papa,lev,level_geom,bl,dm,time)
{

    BL_PROFILE("Castro::Castro()");

    buildMetrics();

    initMFs();

    // initialize the Godunov state array used in hydro -- we wait
    // until here so that ngroups is defined (if needed) in
    // rad_params_module
    ca_init_godunov_indices();

    // NQ will be used to dimension the primitive variable state
    // vector it will include the "pure" hydrodynamical variables +
    // any radiation variables
    NQ = QVAR + QRADVAR;

}

Castro::~Castro ()
{
}

void
Castro::buildMetrics ()
{
    const int ngrd = grids.size();

    radius.resize(ngrd);

    const Real* dx = geom.CellSize();

    for (int i = 0; i < ngrd; i++)
    {
        const Box& b = grids[i];
        int ilo      = b.smallEnd(0)-radius_grow;
        int ihi      = b.bigEnd(0)+radius_grow;
        int len      = ihi - ilo + 1;

        radius[i].resize(len);

        Real* rad = radius[i].dataPtr();

        if (Geometry::IsCartesian())
        {
            for (int j = 0; j < len; j++)
            {
                rad[j] = 1.0;
            }
        }
        else
        {
            RealBox gridloc = RealBox(grids[i],geom.CellSize(),geom.ProbLo());

            const Real xlo = gridloc.lo(0) + (0.5 - radius_grow)*dx[0];

            for (int j = 0; j < len; j++)
            {
                rad[j] = xlo + j*dx[0];
            }
        }
    }

    volume.clear();
    volume.define(grids,dmap,1,NUM_GROW);
    geom.GetVolume(volume);

    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        area[dir].clear();
	area[dir].define(getEdgeBoxArray(dir),dmap,1,NUM_GROW);
        geom.GetFaceArea(area[dir],dir);
    }

    if (level == 0) setGridInfo();
}

// Initialize the MultiFabs and flux registers that live as class members.

void
Castro::initMFs()
{
    fluxes.resize(3);

    for (int dir = 0; dir < BL_SPACEDIM; ++dir)
	fluxes[dir].reset(new MultiFab(getEdgeBoxArray(dir), dmap, NUM_STATE, 0));

    if (level > 0) {

	flux_reg.define(grids, dmap, crse_ratio, level, NUM_STATE);
	flux_reg.setVal(0.0);

    }

    // Set the flux register scalings.

    flux_crse_scale = -1.0;
    flux_fine_scale = 1.0;

}

void
Castro::setTimeLevel (Real time,
                      Real dt_old,
                      Real dt_new)
{
    AmrLevel::setTimeLevel(time,dt_old,dt_new);
}

void
Castro::setGridInfo ()
{

    // Send refinement data to Fortran. We do it here
    // because now the grids have been initialized and
    // we need this data for setting up the problem.
    // Note that this routine will always get called
    // on level 0, even if we are doing a restart,
    // so it is safe to put this here.

    if (level == 0) {

      int max_level = parent->maxLevel();
      int nlevs = max_level + 1;

      Real dx_level[3*nlevs];
      int domlo_level[3*nlevs];
      int domhi_level[3*nlevs];
      int ref_ratio_to_f[3*nlevs];
      int n_error_buf_to_f[nlevs];
      int blocking_factor_to_f[nlevs];

      const Real* dx_coarse = geom.CellSize();

      const int* domlo_coarse = geom.Domain().loVect();
      const int* domhi_coarse = geom.Domain().hiVect();

      for (int dir = 0; dir < 3; dir++) {
	dx_level[dir] = (ZFILL(dx_coarse))[dir];

	domlo_level[dir] = (AMREX_ARLIM_3D(domlo_coarse))[dir];
	domhi_level[dir] = (AMREX_ARLIM_3D(domhi_coarse))[dir];

	// Refinement ratio and error buffer on finest level are meaningless,
	// and we want them to be zero on the finest level because some
	// of the algorithms depend on this feature.

	ref_ratio_to_f[dir + 3 * (nlevs - 1)] = 0;
	n_error_buf_to_f[nlevs-1] = 0;
      }

      for (int lev = 0; lev <= max_level; lev++)
	blocking_factor_to_f[lev] = parent->blockingFactor(lev)[0];

      for (int lev = 1; lev <= max_level; lev++) {
	IntVect ref_ratio = parent->refRatio(lev-1);

	// Note that we are explicitly calculating here what the
	// data would be on refined levels rather than getting the
	// data directly from those levels, because some potential
	// refined levels may not exist at the beginning of the simulation.

	for (int dir = 0; dir < 3; dir++)
	{
	    dx_level[3 * lev + dir] = dx_level[3 * (lev - 1) + dir] / ref_ratio[dir];
	    int ncell = (domhi_level[3 * (lev - 1) + dir] - domlo_level[3 * (lev - 1) + dir] + 1) * ref_ratio[dir];
	    domlo_level[3 * lev + dir] = domlo_level[dir];
	    domhi_level[3 * lev + dir] = domlo_level[3 * lev + dir] + ncell - 1;
	    ref_ratio_to_f[3 * (lev - 1) + dir] = ref_ratio[dir];
	}

	n_error_buf_to_f[lev - 1] = parent->nErrorBuf(lev - 1);
      }

      ca_set_grid_info(max_level, dx_level, domlo_level, domhi_level,
		       ref_ratio_to_f, n_error_buf_to_f, blocking_factor_to_f);

    }

}

void
Castro::initData ()
{
    BL_PROFILE("Castro::initData()");

    //
    // Loop over grids, call FORTRAN function to init with data.
    //
    const Real* dx  = geom.CellSize();
    MultiFab& S_new = get_new_data(State_Type);
    Real cur_time   = state[State_Type].curTime();

    S_new.setVal(0.);

    // make sure dx = dy = dz -- that's all we guarantee to support
    const Real SMALL = 1.e-13;
    if ( (fabs(dx[0] - dx[1]) > SMALL*dx[0]) || (fabs(dx[0] - dx[2]) > SMALL*dx[0]) )
    {
	amrex::Abort("We don't support dx != dy != dz");
    }

    if (verbose && ParallelDescriptor::IOProcessor())
       std::cout << "Initializing the data at level " << level << std::endl;

    {
       for (MFIter mfi(S_new); mfi.isValid(); ++mfi)
       {
          const RealBox& rbx = RealBox(grids[mfi.index()],geom.CellSize(),geom.ProbLo());
          const Box& box     = mfi.validbox();

#pragma gpu
          ca_initdata
              (level, AMREX_INT_ANYD(box.loVect()), AMREX_INT_ANYD(box.hiVect()),
               BL_TO_FORTRAN_ANYD(S_new[mfi]), AMREX_REAL_ANYD(dx),
               AMREX_REAL_ANYD(rbx.lo()), AMREX_REAL_ANYD(rbx.hi()));
       }

       for (MFIter mfi(S_new); mfi.isValid(); ++mfi)
       {
           const Box& box = mfi.validbox();

           // Verify that the sum of (rho X)_i = rho at every cell
#pragma gpu
           ca_check_initial_species(AMREX_INT_ANYD(box.loVect()), AMREX_INT_ANYD(box.hiVect()), BL_TO_FORTRAN_ANYD(S_new[mfi]));
       }

       enforce_consistent_e(S_new);

       // Do a FillPatch so that we can get the ghost zones filled.

       int ng = S_new.nGrow();

       if (ng > 0)
	   AmrLevel::FillPatch(*this, S_new, ng, cur_time, State_Type, 0, S_new.nComp());
    }

    if (verbose && ParallelDescriptor::IOProcessor())
       std::cout << "Done initializing the level " << level << " data " << std::endl;
}

void
Castro::init (AmrLevel &old)
{
    BL_PROFILE("Castro::init(old)");

    Castro* oldlev = (Castro*) &old;

    //
    // Create new grid data by fillpatching from old.
    //
    Real dt_new    = parent->dtLevel(level);
    Real cur_time  = oldlev->state[State_Type].curTime();
    Real prev_time = oldlev->state[State_Type].prevTime();
    Real dt_old    = cur_time - prev_time;
    setTimeLevel(cur_time,dt_old,dt_new);

    for (int s = 0; s < num_state_type; ++s) {
	MultiFab& state_MF = get_new_data(s);
	FillPatch(old, state_MF, state_MF.nGrow(), cur_time, s, 0, state_MF.nComp());
    }

}

//
// This version inits the data on a new level that did not
// exist before regridding.
//
void
Castro::init ()
{
    BL_PROFILE("Castro::init()");

    Real dt        = parent->dtLevel(level);
    Real cur_time  = getLevel(level-1).state[State_Type].curTime();
    Real prev_time = getLevel(level-1).state[State_Type].prevTime();

    Real dt_old = (cur_time - prev_time)/(Real)parent->MaxRefRatio(level-1);

    Real time = cur_time;

    setTimeLevel(time,dt_old,dt);

    for (int s = 0; s < num_state_type; ++s) {
	MultiFab& state_MF = get_new_data(s);
	FillCoarsePatch(state_MF, 0, cur_time, s, 0, state_MF.nComp());
    }
}

Real
Castro::initialTimeStep ()
{
    Real dummy_dt = 0.0;
    Real init_dt  = 0.0;

    init_dt = estTimeStep(dummy_dt);

    return init_dt;
}

Real
Castro::estTimeStep (Real dt_old)
{
    BL_PROFILE("Castro::estTimeStep()");

    if (fixed_dt > 0.0)
        return fixed_dt;

    Real max_dt = 1.e200;

    Real estdt = max_dt;

    const MultiFab& stateMF = get_new_data(State_Type);

    const Real* dx = geom.CellSize();

    // Start the hydro with the max_dt value, but divide by CFL
    // to account for the fact that we multiply by it at the end.
    // This ensures that if max_dt is more restrictive than the hydro
    // criterion, we will get exactly max_dt for a timestep.

    Real estdt_hydro = max_dt / cfl;

#ifdef _OPENMP
#pragma omp parallel
#endif
    {
	Real dt = max_dt / cfl;

	for (MFIter mfi(stateMF,true); mfi.isValid(); ++mfi)
	{
	    const Box& box = mfi.tilebox();

#pragma gpu
            ca_estdt
                (AMREX_INT_ANYD(box.loVect()), AMREX_INT_ANYD(box.hiVect()),
                 BL_TO_FORTRAN_ANYD(stateMF[mfi]),
                 AMREX_REAL_ANYD(dx),
                 AMREX_MFITER_REDUCE_MIN(&dt));
	}
#ifdef _OPENMP
#pragma omp critical (castro_estdt)
#endif
	{
	    estdt_hydro = std::min(estdt_hydro,dt);
	}

    }

    ParallelDescriptor::ReduceRealMin(estdt_hydro);
    estdt_hydro *= cfl;

    estdt = estdt_hydro;

    if (verbose && ParallelDescriptor::IOProcessor())
      std::cout << "Castro::estTimeStep (hydro-limited) at level " << level << ":  estdt = " << estdt << '\n';

    return estdt;
}

void
Castro::computeNewDt (int                   finest_level,
                      int                   sub_cycle,
                      Vector<int>&           n_cycle,
                      const Vector<IntVect>& ref_ratio,
                      Vector<Real>&          dt_min,
                      Vector<Real>&          dt_level,
                      Real                  stop_time,
                      int                   post_regrid_flag)
{
    BL_PROFILE("Castro::computeNewDt()");

    //
    // We are at the start of a coarse grid timecycle.
    // Compute the timesteps for the next iteration.
    //
    if (level > 0)
        return;

    Real change_max = 1.1e0;

    int i;

    Real dt_0 = 1.0e+100;
    int n_factor = 1;
    for (i = 0; i <= finest_level; i++)
    {
        Castro& adv_level = getLevel(i);
        dt_min[i] = adv_level.estTimeStep(dt_level[i]);
    }

    if (fixed_dt <= 0.0)
    {
       if (post_regrid_flag == 1)
       {
          //
          // Limit dt's by pre-regrid dt
          //
          for (i = 0; i <= finest_level; i++)
          {
              dt_min[i] = std::min(dt_min[i],dt_level[i]);
          }
       }
       else
       {
          //
          // Limit dt's by change_max * old dt
          //
          for (i = 0; i <= finest_level; i++)
          {
             if (verbose && ParallelDescriptor::IOProcessor())
                 if (dt_min[i] > change_max*dt_level[i])
                 {
                        std::cout << "Castro::compute_new_dt : limiting dt at level "
                             << i << '\n';
                        std::cout << " ... new dt computed: " << dt_min[i]
                             << '\n';
                        std::cout << " ... but limiting to: "
                             << change_max * dt_level[i] << " = " << change_max
                             << " * " << dt_level[i] << '\n';
                 }
              dt_min[i] = std::min(dt_min[i],change_max*dt_level[i]);
          }
       }
    }

    //
    // Find the minimum over all levels
    //
    for (i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_0 = std::min(dt_0,n_factor*dt_min[i]);
    }

    //
    // Limit dt's by the value of stop_time.
    //
    const Real eps = 0.001*dt_0;
    Real cur_time  = state[State_Type].curTime();
    if (stop_time >= 0.0) {
        if ((cur_time + dt_0) > (stop_time - eps))
            dt_0 = stop_time - cur_time;
    }

    n_factor = 1;
    for (i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_level[i] = dt_0/n_factor;
    }
}

void
Castro::computeInitialDt (int                   finest_level,
                          int                   sub_cycle,
                          Vector<int>&           n_cycle,
                          const Vector<IntVect>& ref_ratio,
                          Vector<Real>&          dt_level,
                          Real                  stop_time)
{
    BL_PROFILE("Castro::computeInitialDt()");

    //
    // Grids have been constructed, compute dt for all levels.
    //
    if (level > 0)
        return;

    int i;

    Real dt_0 = 1.0e+100;
    int n_factor = 1;
    ///TODO/DEBUG: This will need to change for optimal subcycling.
    for (i = 0; i <= finest_level; i++)
    {
        dt_level[i] = getLevel(i).initialTimeStep();
        n_factor   *= n_cycle[i];
        dt_0 = std::min(dt_0,n_factor*dt_level[i]);
    }

    //
    // Limit dt's by the value of stop_time.
    //
    const Real eps = 0.001*dt_0;
    Real cur_time  = state[State_Type].curTime();
    if (stop_time >= 0.0) {
        if ((cur_time + dt_0) > (stop_time - eps))
            dt_0 = stop_time - cur_time;
    }

    n_factor = 1;
    for (i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_level[i] = dt_0/n_factor;
    }
}

void
Castro::post_timestep (int iteration)
{
    BL_PROFILE("Castro::post_timestep()");

    //
    // Integration cycle on fine level grids is complete
    // do post_timestep stuff here.
    //
    int finest_level = parent->finestLevel();

    // Now do the refluxing.

    if (level < parent->finestLevel())
	reflux(level, level+1);

    // Ensure consistency with finer grids.

    if (level < finest_level)
	avgDown();

    MultiFab& S_new = get_new_data(State_Type);

    // Clean up any aberrant state data generated by the reflux and average-down,
    // and then update quantities like temperature to be consistent.

    clean_state(S_new);

    // Flush Fortran output

    if (verbose)
	flush_output();

    if (level == 0)
    {
        int nstep = parent->levelSteps(0);
	Real dtlev = parent->dtLevel(0);
	Real cumtime = parent->cumTime() + dtlev;

	if (sum_interval > 0 && nstep%sum_interval == 0)
	    sum_integrated_quantities();

    }

}

void
Castro::post_regrid (int lbase,
                     int new_finest)
{

    BL_PROFILE("Castro::post_regrid()");

    fine_mask.clear();

}

void
Castro::post_init (Real stop_time)
{
    BL_PROFILE("Castro::post_init()");

    if (level > 0)
        return;

    //
    // Average data down from finer levels
    // so that conserved data is consistent between levels.
    //
    int finest_level = parent->finestLevel();
    for (int k = finest_level-1; k>= 0; k--)
        getLevel(k).avgDown();

        int nstep = parent->levelSteps(0);
	Real dtlev = parent->dtLevel(0);
	Real cumtime = parent->cumTime();
	if (cumtime != 0.0) cumtime += dtlev;

	if (sum_interval > 0 && nstep%sum_interval == 0)
	    sum_integrated_quantities();

}

int
Castro::okToContinue ()
{
    if (level > 0)
        return 1;

    int test = 1;

    return test;
}

void
Castro::FluxRegCrseInit() {

    BL_PROFILE("Castro::FluxRegCrseInit()");

    if (level == parent->finestLevel()) return;

    Castro& fine_level = getLevel(level+1);

    for (int i = 0; i < BL_SPACEDIM; ++i)
	fine_level.flux_reg.CrseInit(*fluxes[i], i, 0, 0, NUM_STATE, flux_crse_scale);

}


void
Castro::FluxRegFineAdd() {

    BL_PROFILE("Castro::FluxRegFineAdd()");

    if (level == 0) return;

    for (int i = 0; i < BL_SPACEDIM; ++i)
	flux_reg.FineAdd(*fluxes[i], i, 0, 0, NUM_STATE, flux_fine_scale);

}


void
Castro::reflux(int crse_level, int fine_level)
{
    BL_PROFILE("Castro::reflux()");

    BL_ASSERT(fine_level > crse_level);

    const Real strt = ParallelDescriptor::second();

    FluxRegister* reg;

    for (int lev = fine_level; lev > crse_level; --lev) {

	reg = &getLevel(lev).flux_reg;

	Castro& crse_lev = getLevel(lev-1);
	Castro& fine_lev = getLevel(lev);

	MultiFab& state = crse_lev.get_new_data(State_Type);

	// Clear out the data that's not on coarse-fine boundaries so that this register only
	// modifies the fluxes on coarse-fine interfaces.

	reg->ClearInternalBorders(crse_lev.geom);

	// Trigger the actual reflux on the coarse level now.

	reg->Reflux(state, crse_lev.volume, 1.0, 0, 0, NUM_STATE, crse_lev.geom);

	// We no longer need the flux register data, so clear it out.

	reg->setVal(0.0);

    }

    if (verbose)
    {
        const int IOProc = ParallelDescriptor::IOProcessorNumber();
        Real      end    = ParallelDescriptor::second() - strt;

#ifdef BL_LAZY
	Lazy::QueueReduction( [=] () mutable {
#endif
        ParallelDescriptor::ReduceRealMax(end,IOProc);
        if (ParallelDescriptor::IOProcessor())
            std::cout << "Castro::reflux() at level " << level << " : time = " << end << std::endl;
#ifdef BL_LAZY
	});
#endif
    }
}

void
Castro::avgDown ()
{

  BL_PROFILE("Castro::avgDown()");

  if (level == parent->finestLevel()) return;

  avgDown(State_Type);

}

void
Castro::normalize_species (MultiFab& S_new)
{

    BL_PROFILE("Castro::normalize_species()");

    int ng = S_new.nGrow();

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new,true); mfi.isValid(); ++mfi)
    {
       const Box& bx = mfi.growntilebox(ng);

#pragma gpu
       ca_normalize_species
           (BL_TO_FORTRAN_ANYD(S_new[mfi]), 
            AMREX_INT_ANYD(bx.loVect()), AMREX_INT_ANYD(bx.hiVect()));
    }

}

void
Castro::enforce_consistent_e (MultiFab& S)
{

  BL_PROFILE("Castro::enforce_consistent_e()");

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S,true); mfi.isValid(); ++mfi)
    {
        const Box& box     = mfi.tilebox();
        const int* lo      = box.loVect();
        const int* hi      = box.hiVect();

#pragma gpu
        ca_enforce_consistent_e(AMREX_INT_ANYD(box.loVect()), AMREX_INT_ANYD(box.hiVect()), BL_TO_FORTRAN_ANYD(S[mfi]));
    }

}

Real
Castro::enforce_min_density (MultiFab& S_old, MultiFab& S_new)
{

    BL_PROFILE("Castro::enforce_min_density()");

    // This routine sets the density in S_new to be larger than the density floor.
    // Note that it will operate everywhere on S_new, including ghost zones.
    // S_old is present so that, after the hydro call, we know what the old density
    // was so that we have a reference for comparison. If you are calling it elsewhere
    // and there's no meaningful reference state, just pass in the same MultiFab twice.

    // The return value is the the negative fractional change in the state that has the
    // largest magnitude. If there is no reference state, this is meaningless.

    Real dens_change = 1.e0;

#ifdef _OPENMP
#pragma omp parallel reduction(min:dens_change)
#endif
    for (MFIter mfi(S_new, true); mfi.isValid(); ++mfi) {

	const Box& bx = mfi.growntilebox();

	FArrayBox& stateold = S_old[mfi];
	FArrayBox& statenew = S_new[mfi];
	FArrayBox& vol      = volume[mfi];

#pragma gpu
	ca_enforce_minimum_density
            (BL_TO_FORTRAN_ANYD(stateold),
             BL_TO_FORTRAN_ANYD(statenew),
             BL_TO_FORTRAN_ANYD(vol),
             AMREX_INT_ANYD(bx.loVect()), AMREX_INT_ANYD(bx.hiVect()),
             AMREX_MFITER_REDUCE_MIN(&dens_change),
             verbose);

    }

    return dens_change;

}

void
Castro::avgDown (int state_indx)
{
    BL_PROFILE("Castro::avgDown(state_indx)");

    if (level == parent->finestLevel()) return;

    Castro& fine_lev = getLevel(level+1);

    const Geometry& fgeom = fine_lev.geom;
    const Geometry& cgeom =          geom;

    MultiFab&  S_crse   = get_new_data(state_indx);
    MultiFab&  S_fine   = fine_lev.get_new_data(state_indx);

    amrex::average_down(S_fine, S_crse,
			 fgeom, cgeom,
			 0, S_fine.nComp(), fine_ratio);
}

void
Castro::allocOldData ()
{
    for (int k = 0; k < num_state_type; k++)
        state[k].allocOldData();
}

void
Castro::removeOldData()
{
    AmrLevel::removeOldData();
}

void
Castro::errorEst (TagBoxArray& tags,
                  int          clearval,
                  int          tagval,
                  Real         time,
                  int          n_error_buf,
                  int          ngrow)
{
    BL_PROFILE("Castro::errorEst()");

    Real t = time;

    // Apply each of the built-in tagging functions.

    for (int j = 0; j < err_list.size(); j++)
	apply_tagging_func(tags, clearval, tagval, t, j);

}



void
Castro::apply_tagging_func(TagBoxArray& tags, int clearval, int tagval, Real time, int j)
{

    const int*  domain_lo = geom.Domain().loVect();
    const int*  domain_hi = geom.Domain().hiVect();
    const Real* dx        = geom.CellSize();
    const Real* prob_lo   = geom.ProbLo();

    for (int j = 0; j < err_list.size(); j++)
    {
        auto mf = derive(err_list[j].name(), time, err_list[j].nGrow());

        BL_ASSERT(mf);

#ifdef _OPENMP
#pragma omp parallel
#endif
	{
	    Vector<int>  itags;

	    for (MFIter mfi(*mf,true); mfi.isValid(); ++mfi)
	    {
		// FABs
		FArrayBox&  datfab  = (*mf)[mfi];
		TagBox&     tagfab  = tags[mfi];

		// tile box
		const Box&  tilebx  = mfi.tilebox();

		// physical tile box
		const RealBox& pbx  = RealBox(tilebx,geom.CellSize(),geom.ProbLo());

		//fab box
		const Box&  datbox  = datfab.box();

		// We cannot pass tagfab to Fortran becuase it is BaseFab<char>.
		// So we are going to get a temporary integer array.
		tagfab.get_itags(itags, tilebx);

		// data pointer and index space
		int*        tptr    = itags.dataPtr();
		const int*  tlo     = tilebx.loVect();
		const int*  thi     = tilebx.hiVect();
		//
		const int*  lo      = tlo;
		const int*  hi      = thi;
		//
		const Real* xlo     = pbx.lo();
		//
		Real*       dat     = datfab.dataPtr();
		const int*  dlo     = datbox.loVect();
		const int*  dhi     = datbox.hiVect();
		const int   ncomp   = datfab.nComp();

		err_list[j].errFunc()(tptr, tlo, thi, &tagval,
				      &clearval, dat, dlo, dhi,
				      lo,hi, &ncomp, domain_lo, domain_hi,
				      dx, xlo, prob_lo, &time, &level);
		//
		// Now update the tags in the TagBox.
		//
                tagfab.tags_and_untags(itags, tilebx);
	    }
	}

    }
}

void
Castro::network_init ()
{
   ca_network_init();
}

void
Castro::extern_init ()
{
  // initialize the external runtime parameters -- these will
  // live in the probin

  if (ParallelDescriptor::IOProcessor()) {
    std::cout << "reading extern runtime parameters ..." << std::endl;
  }

  int probin_file_length = probin_file.length();
  Vector<int> probin_file_name(probin_file_length);

  for (int i = 0; i < probin_file_length; i++)
    probin_file_name[i] = probin_file[i];

  ca_extern_init(probin_file_name.dataPtr(),&probin_file_length);
}

void
Castro::reset_internal_energy(MultiFab& S_new)
{

    BL_PROFILE("Castro::reset_internal_energy()");

    MultiFab old_state;

    int ng = S_new.nGrow();

    int print_fortran_warnings = 0;

    // Ensure (rho e) isn't too small or negative
#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new,true); mfi.isValid(); ++mfi)
    {
        const Box& bx = mfi.growntilebox(ng);

#pragma gpu
        ca_reset_internal_e
            (AMREX_INT_ANYD(bx.loVect()), AMREX_INT_ANYD(bx.hiVect()),
             BL_TO_FORTRAN_ANYD(S_new[mfi]),
             print_fortran_warnings);
    }

    // Flush Fortran output

    if (verbose)
      flush_output();

}

void
Castro::computeTemp(MultiFab& State)
{

  BL_PROFILE("Castro::computeTemp()");

  reset_internal_energy(State);

#ifdef _OPENMP
#pragma omp parallel
#endif
  for (MFIter mfi(State,true); mfi.isValid(); ++mfi)
    {
      const Box& bx = mfi.growntilebox();

#pragma gpu
      ca_compute_temp(AMREX_INT_ANYD(bx.loVect()), AMREX_INT_ANYD(bx.hiVect()), BL_TO_FORTRAN_ANYD(State[mfi]));
    }

}

MultiFab&
Castro::build_fine_mask()
{
    BL_ASSERT(level > 0); // because we are building a mask for the coarser level

    if (!fine_mask.empty()) return fine_mask;

    BoxArray baf = parent->boxArray(level);
    baf.coarsen(crse_ratio);

    const BoxArray& bac = parent->boxArray(level-1);
    const DistributionMapping& dmc = parent->DistributionMap(level-1);
    fine_mask.define(bac,dmc,1,0);
    fine_mask.setVal(1.0);

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(fine_mask); mfi.isValid(); ++mfi)
    {
        FArrayBox& fab = fine_mask[mfi];

	const std::vector< std::pair<int,Box> >& isects = baf.intersections(fab.box());

	for (int ii = 0; ii < isects.size(); ++ii)
	{
	    fab.setVal(0.0,isects[ii].second,0);
	}
    }

    return fine_mask;
}

// Fill a version of the state with ng ghost zones from the state data.
void
Castro::expand_state(MultiFab& S, Real time, int ng)
{

    BL_PROFILE("Castro::expand_state()");

    BL_ASSERT(S.nGrow() >= ng);

    AmrLevel::FillPatch(*this,S,ng,time,State_Type,0,NUM_STATE);

    clean_state(S);

}


void
Castro::check_for_nan(MultiFab& state, int check_ghost)
{

  BL_PROFILE("Castro::check_for_nan()");

  int ng = 0;
  if (check_ghost == 1) {
    ng = state.nComp();
  }

  if (state.contains_nan(Density,state.nComp(),ng,true))
    {
      for (int i = 0; i < state.nComp(); i++)
        {
	  if (state.contains_nan(Density + i, 1, ng, true))
            {
	      std::string abort_string = std::string("State has NaNs in the ") + desc_lst[State_Type].name(i) + std::string(" component::check_for_nan()");
	      amrex::Abort(abort_string.c_str());
            }
        }
    }

}

// Convert a MultiFab with conservative state data u to a primitive MultiFab q.
// Given State_Type state data, perform a number of cleaning steps to make
// sure the data is sensible. The return value is the same as the return
// value of enforce_min_density.

Real
Castro::clean_state(MultiFab& state) {

    BL_PROFILE("Castro::clean_state()");

    // Enforce a minimum density.

    MultiFab temp_state(state.boxArray(), state.DistributionMap(), state.nComp(), state.nGrow());

    MultiFab::Copy(temp_state, state, 0, 0, state.nComp(), state.nGrow());

    Real frac_change = enforce_min_density(temp_state, state);

    // Ensure all species are normalized.

    normalize_species(state);

    // Compute the temperature (note that this will also reset
    // the internal energy for consistency with the total energy).

    computeTemp(state);

    return frac_change;

}
