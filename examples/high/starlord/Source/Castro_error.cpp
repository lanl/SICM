
#include <Castro.H>
#include <Castro_error_F.H>
using std::string;
using namespace amrex;

typedef StateDescriptor::BndryFunc BndryFunc;

void
Castro::ErrorSetUp ()
{
    //
    // DEFINE ERROR ESTIMATION QUANTITIES
    //

    err_list.add("density",1,ErrorRec::Special,ca_denerror);
    err_list.add("Temp",1,ErrorRec::Special,ca_temperror);

}
