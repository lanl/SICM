#include <cstdio>

#include <AMReX_LevelBld.H>
#include <AMReX_ParmParse.H>
#include <Castro.H>
#include <Castro_F.H>
#include <AMReX_buildInfo.H>

using std::string;
using namespace amrex;

static Box the_same_box (const Box& b) { return b; }

typedef StateDescriptor::BndryFunc BndryFunc;

//
// Components are:
//  Interior, Inflow, Outflow,  Symmetry,     SlipWall,     NoSlipWall
//
static int scalar_bc[] =
  {
    INT_DIR, EXT_DIR, FOEXTRAP, REFLECT_EVEN, REFLECT_EVEN, REFLECT_EVEN
  };

static int norm_vel_bc[] =
  {
    INT_DIR, EXT_DIR, FOEXTRAP, REFLECT_ODD,  REFLECT_ODD,  REFLECT_ODD
  };

static int tang_vel_bc[] =
  {
    INT_DIR, EXT_DIR, FOEXTRAP, REFLECT_EVEN, REFLECT_EVEN, REFLECT_EVEN
  };

static
void
set_scalar_bc (BCRec& bc, const BCRec& phys_bc)
{
  const int* lo_bc = phys_bc.lo();
  const int* hi_bc = phys_bc.hi();
  for (int i = 0; i < BL_SPACEDIM; i++)
    {
      bc.setLo(i,scalar_bc[lo_bc[i]]);
      bc.setHi(i,scalar_bc[hi_bc[i]]);
    }
}

static
void
set_x_vel_bc(BCRec& bc, const BCRec& phys_bc)
{
  const int* lo_bc = phys_bc.lo();
  const int* hi_bc = phys_bc.hi();
  bc.setLo(0,norm_vel_bc[lo_bc[0]]);
  bc.setHi(0,norm_vel_bc[hi_bc[0]]);
  bc.setLo(1,tang_vel_bc[lo_bc[1]]);
  bc.setHi(1,tang_vel_bc[hi_bc[1]]);
  bc.setLo(2,tang_vel_bc[lo_bc[2]]);
  bc.setHi(2,tang_vel_bc[hi_bc[2]]);
}

static
void
set_y_vel_bc(BCRec& bc, const BCRec& phys_bc)
{
  const int* lo_bc = phys_bc.lo();
  const int* hi_bc = phys_bc.hi();
  bc.setLo(0,tang_vel_bc[lo_bc[0]]);
  bc.setHi(0,tang_vel_bc[hi_bc[0]]);
  bc.setLo(1,norm_vel_bc[lo_bc[1]]);
  bc.setHi(1,norm_vel_bc[hi_bc[1]]);
  bc.setLo(2,tang_vel_bc[lo_bc[2]]);
  bc.setHi(2,tang_vel_bc[hi_bc[2]]);
}

static
void
set_z_vel_bc(BCRec& bc, const BCRec& phys_bc)
{
  const int* lo_bc = phys_bc.lo();
  const int* hi_bc = phys_bc.hi();
  bc.setLo(0,tang_vel_bc[lo_bc[0]]);
  bc.setHi(0,tang_vel_bc[hi_bc[0]]);
  bc.setLo(1,tang_vel_bc[lo_bc[1]]);
  bc.setHi(1,tang_vel_bc[hi_bc[1]]);
  bc.setLo(2,norm_vel_bc[lo_bc[2]]);
  bc.setHi(2,norm_vel_bc[hi_bc[2]]);
}

void
Castro::variableSetUp ()
{

  // Castro::variableSetUp is called in the constructor of Amr.cpp, so
  // it will get called when we start the job.

  // Output the git commit hashes used to build the executable.

  if (ParallelDescriptor::IOProcessor()) {

    const char* castro_hash = buildInfoGetGitHash(1);
    const char* amrex_hash = buildInfoGetGitHash(2);

    if (strlen(castro_hash) > 0)
      std::cout << "\n" << "Castro git describe: " << castro_hash << "\n";
    if (strlen(amrex_hash) > 0)
      std::cout << "AMReX git describe: " << amrex_hash << "\n";

    std::cout << "\n";
  }

  BL_ASSERT(desc_lst.size() == 0);

  // Get options, set phys_bc
  read_params();

  // Initialize the runtime parameters for any of the external
  // microphysics
  extern_init();

  // Initialize the network
  network_init();

  //
  // Set number of state variables and pointers to components
  //

  int cnt = 0;
  Density = cnt++;
  Xmom = cnt++;
  Ymom = cnt++;
  Zmom = cnt++;
  Eden = cnt++;
  Eint = cnt++;
  Temp = cnt++;

  NumAdv = 0;

  int dm = BL_SPACEDIM;

  // Get the number of species from the network model.
  ca_get_num_spec(&NumSpec);

  if (NumSpec > 0) {
      FirstSpec = cnt;
      cnt += NumSpec;
  }

  NumAux = 0;

  NUM_STATE = cnt;

  // Define NUM_GROW from the F90 module.
  ca_get_method_params(&NUM_GROW);

  // Read in the input values to Fortran.

  ca_set_castro_method_params();

  ca_set_method_params(dm, Density, Xmom, Eden, Eint, Temp, FirstAdv, FirstSpec, FirstAux,
		       NumAdv);

  // Get the number of primitive variables from Fortran.

  ca_get_qvar(&QVAR);
  ca_get_nqaux(&NQAUX);
  ca_get_ngdnv(&NGDNV);

  int coord_type = Geometry::Coord();

  // Get the center variable from the inputs and pass it directly to Fortran.
  Vector<Real> center(BL_SPACEDIM, 0.0);
  ParmParse ppc("castro");
  ppc.queryarr("center",center,0,BL_SPACEDIM);

  ca_set_problem_params(dm,phys_bc.lo(),phys_bc.hi(),
			Interior,Inflow,Outflow,Symmetry,SlipWall,NoSlipWall,coord_type,
			Geometry::ProbLo(),Geometry::ProbHi(),center.dataPtr());

  // Read in the parameters for the tagging criteria
  // and store them in the Fortran module.

  int probin_file_length = probin_file.length();
  Vector<int> probin_file_name(probin_file_length);

  for (int i = 0; i < probin_file_length; i++)
    probin_file_name[i] = probin_file[i];

  ca_get_tagging_params(probin_file_name.dataPtr(),&probin_file_length);

  Interpolater* interp = &cell_cons_interp;

  bool state_data_extrap = false;
  bool store_in_checkpoint;

  int ngrow_state = 0;

  store_in_checkpoint = true;
  desc_lst.addDescriptor(State_Type,IndexType::TheCellType(),
			 StateDescriptor::Point,ngrow_state,NUM_STATE,
			 interp,state_data_extrap,store_in_checkpoint);

//  desc_lst.setDeviceCopy(State_Type, true);

  Vector<BCRec>       bcs(NUM_STATE);
  Vector<std::string> name(NUM_STATE);

  BCRec bc;
  cnt=0; set_scalar_bc(bc,phys_bc); bcs[cnt] = bc; name[cnt] = "density";
  cnt++; set_x_vel_bc(bc,phys_bc);  bcs[cnt] = bc; name[cnt] = "xmom";
  cnt++; set_y_vel_bc(bc,phys_bc);  bcs[cnt] = bc; name[cnt] = "ymom";
  cnt++; set_z_vel_bc(bc,phys_bc);  bcs[cnt] = bc; name[cnt] = "zmom";
  cnt++; set_scalar_bc(bc,phys_bc); bcs[cnt] = bc; name[cnt] = "rho_E";
  cnt++; set_scalar_bc(bc,phys_bc); bcs[cnt] = bc; name[cnt] = "rho_e";
  cnt++; set_scalar_bc(bc,phys_bc); bcs[cnt] = bc; name[cnt] = "Temp";

  // Get the species names from the network model.
  std::vector<std::string> spec_names;
  for (int i = 0; i < NumSpec; i++) {
    int len = 20;
    Vector<int> int_spec_names(len);
    // This call return the actual length of each string in "len"
    ca_get_spec_names(int_spec_names.dataPtr(),&i,&len);
    char char_spec_names[len+1];
    for (int j = 0; j < len; j++)
      char_spec_names[j] = int_spec_names[j];
    char_spec_names[len] = '\0';
    spec_names.push_back(std::string(char_spec_names));
  }

  for (int i=0; i<NumSpec; ++i) {
      cnt++;
      set_scalar_bc(bc,phys_bc);
      bcs[cnt] = bc;
      name[cnt] = "rho_" + spec_names[i];
  }

  desc_lst.setComponent(State_Type,
			Density,
			name,
			bcs,
			BndryFunc(ca_denfill,ca_hypfill));

  num_state_type = desc_lst.size();

  //
  // DEFINE ERROR ESTIMATION QUANTITIES
  //
  ErrorSetUp();

  // method of lines Butcher tableau
#define SECONDORDER_TVD

#ifdef THIRDORDER
  MOL_STAGES = 3;

  a_mol.resize(MOL_STAGES);
  for (int n = 0; n < MOL_STAGES; ++n)
    a_mol[n].resize(MOL_STAGES);

  a_mol[0] = {0,   0, 0};
  a_mol[1] = {0.5, 0, 0};
  a_mol[2] = {-1,  2, 0};

  b_mol = {1./6., 2./3., 1./6.};

  c_mol = {0.0, 0.5, 1};
#endif

#ifdef THIRDORDER_TVD
  MOL_STAGES = 3;

  a_mol.resize(MOL_STAGES);
  for (int n = 0; n < MOL_STAGES; ++n)
    a_mol[n].resize(MOL_STAGES);

  a_mol[0] = {0.0,  0.0,  0.0};
  a_mol[1] = {1.0,  0.0,  0.0};
  a_mol[2] = {0.25, 0.25, 0.0};

  b_mol = {1./6., 1./6., 2./3.};

  c_mol = {0.0, 1.0, 0.5};
#endif

#ifdef SECONDORDER
  MOL_STAGES = 2;

  a_mol.resize(MOL_STAGES);
  for (int n = 0; n < MOL_STAGES; ++n)
    a_mol[n].resize(MOL_STAGES);

  a_mol[0] = {0,   0,};
  a_mol[1] = {0.5, 0,};

  b_mol = {0.0, 1.0};

  c_mol = {0.0, 0.5};
#endif

#ifdef SECONDORDER_TVD
  MOL_STAGES = 2;

  a_mol.resize(MOL_STAGES);
  for (int n = 0; n < MOL_STAGES; ++n)
    a_mol[n].resize(MOL_STAGES);

  a_mol[0] = {0,   0,};
  a_mol[1] = {1.0, 0,};

  b_mol = {0.5, 0.5};

  c_mol = {0.0, 1.0};
#endif

#ifdef FIRSTORDER
  MOL_STAGES = 1;

  a_mol.resize(MOL_STAGES);
  for (int n = 0; n < MOL_STAGES; ++n)
    a_mol[n].resize(MOL_STAGES);

  a_mol[0] = {1};
  b_mol = {1.0};
  c_mol = {0.0};
#endif

}
