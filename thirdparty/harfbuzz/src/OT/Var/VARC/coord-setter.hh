#ifndef OT_VAR_VARC_COORD_SETTER_HH
#define OT_VAR_VARC_COORD_SETTER_HH


#include "../../../hb.hh"


namespace OT {
//namespace Var {


struct coord_setter_t
{
  coord_setter_t (hb_array_t<const int> coords) :
    coords (coords) {}

  int& operator [] (unsigned idx)
  {
    if (unlikely (idx >= HB_VAR_COMPOSITE_MAX_AXES))
      return Crap(int);
    if (coords.length < idx + 1)
      coords.resize (idx + 1);
    return coords[idx];
  }

  hb_array_t<int> get_coords ()
  { return coords.as_array (); }

  hb_vector_t<int> coords;
};


//} // namespace Var

} // namespace OT

#endif /* OT_VAR_VARC_COORD_SETTER_HH */
