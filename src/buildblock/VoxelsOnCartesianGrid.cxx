//
// $Id$
//

/*!
  \file 
  \buildblock 
  \brief Implementations of VoxelsOnCartesianGrid 

  \author Sanida Mustafovic 
  \author Kris Thielemans (with help from Alexey Zverovich)
  \author PARAPET project

  $Date$
  $Revision$

*/

#include "VoxelsOnCartesianGrid.h"
#include "IndexRange3D.h"
#include "CartesianCoordinate3D.h"
#include "utilities.h"
#include "ProjDataInfoCylindricalArcCorr.h"
#include "Scanner.h"
#include "Bin.h"
#include "tomo/round.h"
#include <fstream>
#include <algorithm>
#ifndef TOMO_NO_NAMESPACES
using std::ifstream;
using std::max;
#endif

START_NAMESPACE_TOMO

// a local help function to find appropriate sizes etc.

static void find_sampling_and_z_size(
				 float& z_sampling,
				 float& s_sampling,
				 int& z_size,
				 const ProjDataInfo* proj_data_info_ptr)
{

  // first z- things

  if (const ProjDataInfoCylindrical*
        proj_data_info_cyl_ptr = 
	dynamic_cast<const ProjDataInfoCylindrical*>(proj_data_info_ptr))

   {
    // the case of cylindrical data

    z_sampling = proj_data_info_cyl_ptr->get_ring_spacing()/2;
     
    // for 'span>1' case, we take z_size = number of sinograms in segment 0
    // for 'span==1' case, we take 2*num_rings-1

    // first check if we have segment 0
    assert(proj_data_info_cyl_ptr->get_min_segment_num() <= 0);
    assert(proj_data_info_cyl_ptr->get_max_segment_num() >= 0);

    z_size = 
      proj_data_info_cyl_ptr->get_max_ring_difference(0) >
      proj_data_info_cyl_ptr->get_min_ring_difference(0)
      ? proj_data_info_cyl_ptr->get_num_axial_poss(0)
      : 2*proj_data_info_cyl_ptr->get_num_axial_poss(0) - 1;
  }
  else
  {
    // this is any other weird projection data. We just check sampling of segment 0
    
    // first check if we have segment 0
    assert(proj_data_info_cyl_ptr->get_min_segment_num() <= 0);
    assert(proj_data_info_cyl_ptr->get_max_segment_num() >= 0);

    // TODO make this independent on segment etc.
    z_sampling = 
      proj_data_info_ptr->get_sampling_in_t(Bin(0,0,1,0));

    z_size = proj_data_info_ptr->get_num_axial_poss(0);
  }

  // now do s_sampling
  // TODO make this independent on segment etc.
    s_sampling = 
      proj_data_info_ptr->get_sampling_in_s(Bin(0,0,0,1));

}

template<class elemT>   		            		      
VoxelsOnCartesianGrid<elemT>::VoxelsOnCartesianGrid(const ProjDataInfo& proj_data_info,
						    const float zoom, 
						    const CartesianCoordinate3D<float>& origin,
						    const int xy_size)
						    
{
  set_origin(origin);

  // initialise to 0 to prevent compiler warnings
  int z_size = 0;
  float z_sampling = 0;
  float s_sampling = 0;
  find_sampling_and_z_size(z_sampling, s_sampling, z_size, &proj_data_info);
  
  set_grid_spacing(
      CartesianCoordinate3D<float>(z_sampling, s_sampling/zoom, s_sampling/zoom)
      );
    
  int xy_size_used = xy_size;

  if (xy_size==-1)
    {
      // default it to cover full FOV by taking 2*R_in_pixs+1
      const float FOVradius_in_mm = 
	max(proj_data_info.get_s(Bin(0,0,0,proj_data_info.get_max_tangential_pos_num())),
	    -proj_data_info.get_s(Bin(0,0,0,proj_data_info.get_min_tangential_pos_num())));
      xy_size_used = round(2*FOVradius_in_mm / get_voxel_size().x() + 1);
    }
  if (xy_size_used<0)
    error("VoxelsOnCartesianGrid: attempt to construct image with negative xy_size %d\n", 
	  xy_size_used);
  if (xy_size_used==0)
    warning("VoxelsOnCartesianGrid: constructed image with xy_size 0\n");

  IndexRange3D range (0, z_size-1, 
		      -(xy_size_used/2), -(xy_size_used/2) + xy_size_used-1,
		      -(xy_size_used/2), -(xy_size_used/2) + xy_size_used-1);
  
  grow(range);
  
}

template<class elemT>
VoxelsOnCartesianGrid<elemT>::VoxelsOnCartesianGrid(const ProjDataInfo& proj_data_info,
						    const float zoom, 
						    const CartesianCoordinate3D<float>& origin,
						    const bool make_xy_size_odd)
						    
{
  set_origin(origin);
  // initialise to 0 to prevent compiler warnings
  int z_size = 0;
  float z_sampling = 0;
  float s_sampling = 0;
  find_sampling_and_z_size(z_sampling, s_sampling, z_size,&proj_data_info);

   
  set_grid_spacing(
      CartesianCoordinate3D<float>(z_sampling, s_sampling/zoom, s_sampling/zoom)
      );
  
  int xy_size = -1;
  {
    // default it to cover full FOV
    const float FOVradius_in_mm = 
      max(proj_data_info.get_s(Bin(0,0,0,proj_data_info.get_max_tangential_pos_num())),
	  -proj_data_info.get_s(Bin(0,0,0,proj_data_info.get_min_tangential_pos_num())));
    xy_size = round(2*FOVradius_in_mm / get_voxel_size().x());
  }
  if (make_xy_size_odd && (xy_size%2 == 0))
    xy_size++;
  
  IndexRange3D range (0, z_size-1, 
    -(xy_size/2), -(xy_size/2) + xy_size-1,
    -(xy_size/2), -(xy_size/2) + xy_size-1);
  
  grow(range);
  

} 


/*!
  This member function will be unnecessary when all compilers can handle
  'covariant' return types. 
  It is a non-virtual counterpart of get_empty_voxels_on_cartesian_grid.
*/
template<class elemT>
VoxelsOnCartesianGrid<elemT>*
VoxelsOnCartesianGrid<elemT>::get_empty_voxels_on_cartesian_grid() const

{
  return new VoxelsOnCartesianGrid(get_index_range(),
		                   get_origin(), 
		                   get_grid_spacing());
}

template<class elemT>
DiscretisedDensity<3, elemT>* 
VoxelsOnCartesianGrid<elemT>::clone() const
{
  return new VoxelsOnCartesianGrid(*this);
}

template<class elemT>  
PixelsOnCartesianGrid<elemT>  		            		      
VoxelsOnCartesianGrid<elemT>::get_plane(const int z) const
{
  PixelsOnCartesianGrid<elemT> 
    plane(operator[](z),
          get_origin(),
	  Coordinate2D<float>(get_voxel_size().y(), get_voxel_size().x())
	  );
  return plane;
}

/*! This function requires that the dimensions, origin and grid_spacings match. */
template<class elemT>   
void		            		      
VoxelsOnCartesianGrid<elemT>::set_plane(const PixelsOnCartesianGrid<elemT>& plane, const int z)
{
  assert(get_min_x() == plane.get_min_x());
  assert(get_max_x() == plane.get_max_x());
  assert(get_min_y() == plane.get_min_y());
  assert(get_max_y() == plane.get_max_y());
  assert(get_origin() == plane.get_origin());
  assert(get_voxel_size().x() == plane.get_pixel_size().x());
  assert(get_voxel_size().y() == plane.get_pixel_size().y());
  
  operator[](z) = plane;	  
}

template<class elemT>   
void		            		      
VoxelsOnCartesianGrid<elemT>::grow_z_range(const int min_z, const int max_z)
{
  /* This is somewhat complicated as Array is not very good with regular ranges.
     It works by 
     - getting the regular range, 
     - 'grow' this by hand, 
     - make a general IndexRange from this
     - call Array::grow with the general range
  */
  CartesianCoordinate3D<int> min_indices;
  CartesianCoordinate3D<int> max_indices;

  get_regular_range(min_indices, max_indices);
  assert(min_z <= min_indices.z());
  assert(max_z >= max_indices.z());
  min_indices.z() = min_z;
  max_indices.z() = max_z;
  grow(IndexRange<3>(min_indices, max_indices));
}

/****************************
 static members
 ***************************/
template<class elemT>
VoxelsOnCartesianGrid<elemT> VoxelsOnCartesianGrid<elemT>::ask_parameters()
{
  // this is completely superseded by read_from_file
  // TODO make into something else useful?

  // Open file with data
  ifstream input;
  
  ask_filename_and_open(
    input, "Enter filename for input image", ".v", 
    ios::in | ios::binary);

   Scanner * scanner_ptr = 
    Scanner::ask_parameters();


  NumericType data_type;
  {
    int data_type_sel = ask_num("Type of data :\n\
0: signed 16bit int, 1: unsigned 16bit int, 2: 4bit float ", 0,2,2);
    switch (data_type_sel)
      { 
      case 0:
	data_type = NumericType::SHORT;
	break;
      case 1:
	data_type = NumericType::USHORT;
	break;
      case 2:
	data_type = NumericType::FLOAT;
	break;
      }
  }


  {
    // find offset 

    input.seekg(0L, ios::beg);   
    unsigned long file_size = find_remaining_size(input);

    unsigned long offset_in_file = ask_num("Offset in file (in bytes)", 
			     0UL,file_size, 0UL);
    input.seekg(offset_in_file, ios::beg);
  }

  
  CartesianCoordinate3D<float> 
    origin(0,0,0);
  CartesianCoordinate3D<float>
    voxel_size(scanner_ptr->get_ring_spacing()/2,
               scanner_ptr->get_default_bin_size(),
               scanner_ptr->get_default_bin_size()); 
  int num_bins_from_scanner = scanner_ptr->get_default_num_arccorrected_bins();
 int num_rings_from_scanner = scanner_ptr->get_num_rings();
  int max_bin = (- num_bins_from_scanner /2) +  num_bins_from_scanner -1;
  if (num_bins_from_scanner % 2 == 0 &&
      ask("Make x,y size odd ?", true))
    max_bin++;
   

  VoxelsOnCartesianGrid<elemT> 
    input_image(IndexRange3D(
				0, 2*num_rings_from_scanner-2,
				(-num_bins_from_scanner/2), max_bin,
				(-num_bins_from_scanner/2), max_bin),
		origin,
		voxel_size);


  // TODO handle scale factor in case of not reading float
  float scale = float(1);
  input_image.read_data(input, data_type, scale);  
  assert(scale==1);

  return input_image; 

}

/**********************************************
 instantiations
 **********************************************/
template VoxelsOnCartesianGrid<float>;

END_NAMESPACE_TOMO
