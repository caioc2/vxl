// This is brl/bseg/boxm2/pro/processes/boxm2_import_point_cloud_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief  A process for assigning colors to the vertices of a given mesh.
// There is also a threshold prob_t on probability of points to output.
// Points with lower probability than prob_t are assigned a color of (0,0,0).
//
// \author Ali Osman Ulusoy
// \date Mar 02, 2012

#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_util.h>
#include <boxm2/io/boxm2_cache.h>

#include <bvrml/bvrml_write.h>
#include <boxm2/cpp/algo/boxm2_gauss_rgb_processor.h>

#include <bmsh3d/algo/bmsh3d_fileio.h>
#include <boct/boct_bit_tree.h>

#include <vcl_fstream.h>
#include <vcl_cassert.h>



void readPointsFromXYZ(const vcl_string& filename, vcl_vector<vgl_point_3d<double> > & pts)
{
  if(vul_file::extension(filename) == ".xyz")
  {
      vcl_ifstream ifile(filename.c_str());
      vcl_string line;
      while(vcl_getline(ifile,line))
      {
          vcl_istringstream iss(line);
          vnl_vector<double> x;
          iss>>x;
          pts.push_back(vgl_point_3d<double>(x[0],x[1],x[2]) );
      }
      ifile.close();
  }
  else{

      vcl_cout<<"Point Cloud is empty"<<vcl_endl;
      return ;
  }
}

namespace boxm2_import_point_cloud_process_globals
{
  const unsigned n_inputs_ = 3;
  const unsigned n_outputs_ = 0;
}

bool boxm2_import_point_cloud_process_cons(bprb_func_process& pro)
{
  using namespace boxm2_import_point_cloud_process_globals;

  //process takes 4 or 5 inputs and no outputs
  vcl_vector<vcl_string> input_types_(n_inputs_);
  input_types_[0] = "boxm2_scene_sptr";
  input_types_[1] = "boxm2_cache_sptr";
  input_types_[2] = "vcl_string"; //input ply filename
  vcl_vector<vcl_string>  output_types_(n_outputs_);
  return pro.set_input_types(input_types_)
      && pro.set_output_types(output_types_);
}

bool boxm2_import_point_cloud_process(bprb_func_process& pro)
{
  using namespace boxm2_import_point_cloud_process_globals;

  if ( pro.n_inputs() < n_inputs_ ) {
    vcl_cout << pro.name() << ": The input number should be " << n_inputs_<< vcl_endl;
    return false;
  }

  //get the inputs
  unsigned i = 0;
  boxm2_scene_sptr scene = pro.get_input<boxm2_scene_sptr>(i++);
  boxm2_cache_sptr cache = pro.get_input<boxm2_cache_sptr>(i++);
  vcl_string input_mesh_filename = pro.get_input<vcl_string>(i++);
  vcl_vector<vgl_point_3d<double> > all_points;
  readPointsFromXYZ(input_mesh_filename, all_points);
  vgl_point_3d<double> local;
  boxm2_block_id id;
  for (unsigned  i = 0; i < all_points.size(); i++) {
    if (!scene->contains(all_points[i], id, local)) {
      //vcl_cout << "ERROR: point: " << all_points[i] << " isn't in scene. Exiting...." << vcl_endl;
      //return false;
    }
    else
    {
    int index_x=(int)vcl_floor(local.x());
    int index_y=(int)vcl_floor(local.y());
    int index_z=(int)vcl_floor(local.z());
    boxm2_block * blk=cache->get_block(id);
    boxm2_block_metadata mdata = scene->get_block_metadata_const(id);
    vnl_vector_fixed<unsigned char,16> treebits=blk->trees()(index_x,index_y,index_z);
    boct_bit_tree tree(treebits.data_block(),mdata.max_level_);
    int bit_index=tree.traverse(local);
    int depth=tree.depth_at(bit_index);
    if(depth >= 2 )
    {
    int data_offset=tree.get_data_index(bit_index,false);
    boxm2_data_base *  alpha_base  = cache->get_data_base(id,boxm2_data_traits<BOXM2_ALPHA>::prefix());
    alpha_base->enable_write();
    boxm2_data<BOXM2_ALPHA> *alpha_data=new boxm2_data<BOXM2_ALPHA>(alpha_base->data_buffer(),alpha_base->buffer_length(),alpha_base->block_id());
    double side_len = 1.0 / (double) (1 << depth);
    //store cell probability
    double alpha = - vcl_log(1.0f-0.99f) / (side_len * mdata.sub_block_dim_.x()) ;
    alpha_data->data()[data_offset] = alpha ;
    }
    }
  }
  return true;
}
