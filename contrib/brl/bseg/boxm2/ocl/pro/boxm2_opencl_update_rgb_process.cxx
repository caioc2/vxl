
#include "boxm2_opencl_update_rgb_process.h"

//boxm2 data structures
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/boxm2_data_base.h>
#include <vil/vil_save.h>
#include <vil/vil_math.h>
#include <vil/vil_load.h>
#include <boxm2/ocl/boxm2_ocl_util.h>

//brdb stuff
#include <brdb/brdb_value.h>

//directory utility
#include <vul/vul_timer.h>
#include <vcl_where_root_dir.h>

//TODO IN THIS INIT METHOD: Need to pass in a ref to the OPENCL_CACHE so this
//class can easily access BOCL_MEMs
bool boxm2_opencl_update_rgb_process::init_kernel(cl_context* context,
                                              cl_device_id* device,
                                              vcl_string opts)
{
  context_ = context;

  vcl_vector<vcl_string> src_paths;
  vcl_string source_dir = vcl_string(VCL_SOURCE_ROOT_DIR) + "/contrib/brl/bseg/boxm2/ocl/cl/";
  src_paths.push_back(source_dir + "scene_info.cl");
  src_paths.push_back(source_dir + "cell_utils.cl");
  src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
  src_paths.push_back(source_dir + "backproject.cl");
  src_paths.push_back(source_dir + "statistics_library_functions.cl");
  src_paths.push_back(source_dir + "ray_bundle_library_opt.cl");
  src_paths.push_back(source_dir + "bit/update_rgb_kernels.cl");
  vcl_vector<vcl_string> non_ray_src = vcl_vector<vcl_string>(src_paths); 
  src_paths.push_back(source_dir + "update_rgb_functors.cl");
  src_paths.push_back(source_dir + "bit/cast_ray_bit.cl");

  //compilation options
  vcl_string options = " -D INTENSITY ";
  options += " -D ATOMIC_OPT "; 
  options += opts;

  //create all passes
  bocl_kernel* seg_len = new bocl_kernel();
  vcl_string seg_opts = options + " -D SEGLEN -D STEP_CELL=step_cell_seglen(aux_args,data_ptr,llid,d) "; 
  seg_len->create_kernel(context_, device, src_paths, "seg_len_main", seg_opts, "update::seg_len (rgb)");
  update_kernels_.push_back(seg_len);

  //create  compress rgb pass
  bocl_kernel* comp = new bocl_kernel();
  vcl_string comp_opts = options + " -D COMPRESS_RGB "; 
  comp->create_kernel(context_, device, non_ray_src, "compress_rgb", comp_opts, "update::compress_rgb");
  update_kernels_.push_back(comp);

  bocl_kernel* pre_inf = new bocl_kernel();
  vcl_string pre_opts = options + " -D PREINF -D STEP_CELL=step_cell_preinf(aux_args,data_ptr,llid,d) "; 
  pre_inf->create_kernel(context_, device, src_paths, "pre_inf_main", pre_opts, "update::pre_inf (rgb)");
  update_kernels_.push_back(pre_inf);

  //may need DIFF LIST OF SOURCES FOR THIS GUY
  bocl_kernel* proc_img = new bocl_kernel();
  proc_img->create_kernel(context_, device, non_ray_src, "proc_norm_image", options, "update::proc_norm_image (rgb)");
  update_kernels_.push_back(proc_img);

  //push back cast_ray_bit
  bocl_kernel* bayes_main = new bocl_kernel();
  vcl_string bayes_opt = options + " -D BAYES -D STEP_CELL=step_cell_bayes(aux_args,data_ptr,llid,d) "; 
  bayes_main->create_kernel(context_, device, src_paths, "bayes_main", bayes_opt, "update::bayes_main (rgb)");
  update_kernels_.push_back(bayes_main);

  //may need DIFF LIST OF SOURCES FOR THSI GUY TOO
  bocl_kernel* update = new bocl_kernel();
  update->create_kernel(context_, device, non_ray_src, "update_bit_scene_main", options, "update::update_main (rgb)");
  update_kernels_.push_back(update);

  return true;
}


// Opencl Update Process
// arguments will be (should be)
// * scene pointer
// * camera (for input image)
// * input image
// * visibility image...
bool boxm2_opencl_update_rgb_process::execute(vcl_vector<brdb_value_sptr>& input, vcl_vector<brdb_value_sptr>& output)
{
  transfer_time_ = 0.0f; gpu_time_ = 0.0f; total_time_ = 0.0f;
  vul_timer total;
  int i = 0;

  //scene argument
  brdb_value_t<boxm2_scene_sptr>* scene_brdb = static_cast<brdb_value_t<boxm2_scene_sptr>* >( input[i++].ptr() );
  boxm2_scene_sptr scene = scene_brdb->value();

  //camera
  brdb_value_t<vpgl_camera_double_sptr>* brdb_cam = static_cast<brdb_value_t<vpgl_camera_double_sptr>* >( input[i++].ptr() );
  vpgl_camera_double_sptr cam = brdb_cam->value();
  cl_float* cam_buffer = new cl_float[16*3];
  boxm2_ocl_util::set_persp_camera(cam, cam_buffer);
  persp_cam_ = new bocl_mem((*context_), cam_buffer, 3*sizeof(cl_float16), "persp cam buffer");
  persp_cam_->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //input image buffer
  brdb_value_t<vil_image_view_base_sptr>* brdb_img = static_cast<brdb_value_t<vil_image_view_base_sptr>* >( input[i++].ptr() );
  vil_image_view_base_sptr img = brdb_img->value();
  vil_image_view<vil_rgba<vxl_byte> >* img_view = static_cast<vil_image_view<vil_rgba<vxl_byte> >* >(img.ptr());
  //vil_image_view<float>* img_view = static_cast<vil_image_view<float>* >(img.ptr());
  this->write_input_image(img_view);
  
  //store data type
  brdb_value_t<vcl_string>* brdb_data_type = static_cast<brdb_value_t<vcl_string>* >( input[i++].ptr() );
  data_type_=brdb_data_type->value();
  
  //exp image dimensions
  img_size_[0] = img_view->ni();
  img_size_[1] = img_view->nj();
  int* img_dim_buff = new int[4];
  img_dim_buff[0] = 0;
  img_dim_buff[1] = 0;
  img_dim_buff[2] = img_view->ni();
  img_dim_buff[3] = img_view->nj();
  img_dim_ = new bocl_mem((*context_), img_dim_buff, sizeof(cl_int4), "image dims");
  img_dim_->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //output buffer
  float* output_arr = new float[500];
  for (int i=0; i<500; ++i) output_arr[i] = 0.0f;
  cl_output_ = new bocl_mem((*context_), output_arr, sizeof(float)*500, "output buffer");
  cl_output_->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //bit lookup buffer
  cl_uchar* lookup_arr = new cl_uchar[256];
  boxm2_ocl_util::set_bit_lookup(lookup_arr);
  lookup_ = new bocl_mem((*context_), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
  lookup_->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  // app density used for proc_norm_image
  float* app_buffer = new float[4];
  app_buffer[0] = 1.0f;
  app_buffer[1] = 0.0f;
  app_buffer[2] = 0.0f;
  app_buffer[3] = 0.0f;
  app_density_ = new bocl_mem((*context_), app_buffer, sizeof(cl_float4), "app density buffer");
  app_density_->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //For each ID in the visibility order, grab that block
  vcl_vector<boxm2_block_id> vis_order = scene->get_vis_blocks( (vpgl_perspective_camera<double>*) cam.ptr());
  vcl_vector<boxm2_block_id>::iterator id;

  //Go through each kernel, execute on each block

  for (unsigned int i=0; i<update_kernels_.size(); ++i)
  {
#if 1
    vcl_cout<<"UPDATE KERNEL : "<<update_kernels_[i]->id()<<", datatype: "<<data_type_<<vcl_endl;
#endif
    if ( i == UPDATE_PROC ) {
      this->set_workspace(i);
      this->set_args(i);

      //execute kernel
      update_kernels_[i]->execute( (*command_queue_), 2, lThreads_, gThreads_);
      int status = clFinish(*command_queue_);
      check_val(status, MEM_FAILURE, "UPDATE EXECUTE FAILED: " + error_to_string(status));
      update_kernels_[i]->clear_args();
      image_->read_to_buffer(*command_queue_);
      continue;
    }

    //zip through visible blocks, and execute this pass's kernel
    for (id = vis_order.begin(); id != vis_order.end(); ++id)
    {
      //write the image values to the buffer
      vul_timer transfer;
      blk_       = cache_->get_block(*id);
      alpha_     = cache_->get_data<BOXM2_ALPHA>(*id);
      //mog_       = cache_->get_data<BOXM2_MOG2_RGB>(*id); 
      if (data_type_=="8bit")
        mog_       = cache_->get_data<BOXM2_MOG3_GREY>(*id);
      else if (data_type_=="16bit")
        mog_       = cache_->get_data<BOXM2_MOG3_GREY_16>(*id);
      num_obs_   = cache_->get_data<BOXM2_NUM_OBS>(*id);
      blk_info_  = cache_->loaded_block_info();

      //get aux data
      aux_       = cache_->get_data<BOXM2_AUX>(*id);
      transfer_time_ += (float) transfer.all();

      //set workspace and args for this pass
      this->set_workspace(i);
      this->set_args(i);

      //execute kernel
      update_kernels_[i]->execute( (*command_queue_), 2, lThreads_, gThreads_);
      int status = clFinish(*command_queue_);
      check_val(status, MEM_FAILURE, "UPDATE EXECUTE FAILED: " + error_to_string(status));
      //gpu_time_ += update_kernels_[i]->exec_time();

      //clear render kernel args so it can reset em on next execution
      update_kernels_[i]->clear_args();

      //write info to disk
      blk_->read_to_buffer(*command_queue_);
      alpha_->read_to_buffer(*command_queue_);
      mog_->read_to_buffer(*command_queue_);
      num_obs_->read_to_buffer(*command_queue_);
      aux_->read_to_buffer(*command_queue_);

      //read image out to buffer (from gpu)
      image_->read_to_buffer(*command_queue_);
      cl_output_->read_to_buffer(*command_queue_);
      clFinish(*command_queue_);
    }
  }

  //clean up camera, lookup_arr, img_dim_buff
  delete[] output_arr;
  delete[] img_dim_buff;
  delete[] lookup_arr;
  delete[] cam_buffer;
  delete[] app_buffer;

  delete cl_output_;
  delete persp_cam_;
  delete img_dim_;
  delete lookup_;
  delete app_density_;

  //record total time
  total_time_ = (float) total.all();
  return true;
}

bool boxm2_opencl_update_rgb_process::clean()
{
  return true;
}

bool boxm2_opencl_update_rgb_process::set_workspace(unsigned pass)
{
  switch (pass) {
    case UPDATE_SEGLEN:
    case UPDATE_PREINF:
    case UPDATE_PROC:
    case UPDATE_BAYES:
      lThreads_[0]  = 8;
      lThreads_[1]  = 8;
      gThreads_[0] = RoundUp(img_size_[0],lThreads_[0]);
      gThreads_[1] = RoundUp(img_size_[1],lThreads_[1]);
      break;
    case UPDATE_COMPRESS_RGB:
    case UPDATE_CELL:
    {
      boxm2_scene_info* info_buffer = (boxm2_scene_info*) blk_info_->cpu_buffer();
      int numbuf = info_buffer->num_buffer;
      int datlen = info_buffer->data_buffer_length;
      gThreads_[0] = RoundUp(numbuf*datlen,64);
      gThreads_[1] = 1;
      lThreads_[0]  = 64;
      lThreads_[1]  = 1;
      break;
    }
  }
  return true;
}


bool boxm2_opencl_update_rgb_process::set_args(unsigned pass)
{
  switch (pass)
  {
    case UPDATE_SEGLEN :
      update_kernels_[pass]->set_arg( blk_info_ );
      update_kernels_[pass]->set_arg( blk_ );
      update_kernels_[pass]->set_arg( alpha_ );
      update_kernels_[pass]->set_arg( num_obs_ );
      update_kernels_[pass]->set_arg( aux_ );
      update_kernels_[pass]->set_arg( lookup_ );
      update_kernels_[pass]->set_arg( persp_cam_ );
      update_kernels_[pass]->set_arg( img_dim_ );
      update_kernels_[pass]->set_arg( image_ );
      update_kernels_[pass]->set_arg( cl_output_ );
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_uchar16) );//local tree,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_uchar4) ); //ray bundle,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_int) );    //cell pointers,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_float4) ); //cached aux,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*10*sizeof(cl_uchar) ); //cumsum buffer, imindex buffer
      break;
    case UPDATE_COMPRESS_RGB : 
      update_kernels_[pass]->set_arg( blk_info_ );
      update_kernels_[pass]->set_arg( aux_ );
      break;
    case UPDATE_PREINF :
      update_kernels_[pass]->set_arg( blk_info_ );
      update_kernels_[pass]->set_arg( blk_ );
      update_kernels_[pass]->set_arg( alpha_ );
      update_kernels_[pass]->set_arg( mog_ );
      update_kernels_[pass]->set_arg( num_obs_ );
      update_kernels_[pass]->set_arg( aux_ );
      update_kernels_[pass]->set_arg( lookup_ );
      update_kernels_[pass]->set_arg( persp_cam_ );
      update_kernels_[pass]->set_arg( img_dim_ );
      update_kernels_[pass]->set_arg( image_ );
      update_kernels_[pass]->set_arg( cl_output_ );
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_uchar16) );//local tree,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*10*sizeof(cl_uchar) ); //cumsum buffer, imindex buffer
      break;
    case UPDATE_PROC :
      update_kernels_[pass]->set_arg( image_ );
      update_kernels_[pass]->set_arg( app_density_ );
      update_kernels_[pass]->set_arg( img_dim_ );
      break;
    case UPDATE_BAYES :
      update_kernels_[pass]->set_arg( blk_info_ );
      update_kernels_[pass]->set_arg( blk_ );
      update_kernels_[pass]->set_arg( alpha_ );
      update_kernels_[pass]->set_arg( mog_ );
      update_kernels_[pass]->set_arg( num_obs_ );
      update_kernels_[pass]->set_arg( aux_ );
      update_kernels_[pass]->set_arg( lookup_ );
      update_kernels_[pass]->set_arg( persp_cam_ );
      update_kernels_[pass]->set_arg( img_dim_ );
      update_kernels_[pass]->set_arg( image_ );
      update_kernels_[pass]->set_arg( cl_output_ );
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_uchar16) );//local tree,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_short2) ); //ray bundle,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_int) );    //cell pointers,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*sizeof(cl_float) ); //cached aux,
      update_kernels_[pass]->set_local_arg( lThreads_[0]*lThreads_[1]*10*sizeof(cl_uchar) ); //cumsum buffer, imindex buffer
      break;
    case UPDATE_CELL :
      update_kernels_[pass]->set_arg( blk_info_ );
      update_kernels_[pass]->set_arg( alpha_ );
      update_kernels_[pass]->set_arg( mog_ );
      update_kernels_[pass]->set_arg( num_obs_ );
      update_kernels_[pass]->set_arg( aux_ );
      update_kernels_[pass]->set_arg( cl_output_ );
      break;
  }
  return true;
}


//Gonna write RGB A values to last four of float8
bool boxm2_opencl_update_rgb_process::write_input_image(vil_image_view<vil_rgba<vxl_byte> >* input_image)
{
  //write to buffer (or create it)
  unsigned ni=RoundUp(input_image->ni(),8);
  unsigned nj=RoundUp(input_image->nj(),8);
  int numFloats = 8; 
  float* buff = (image_) ? (float*) image_->cpu_buffer() : new float[numFloats * ni*nj];
  int count=0;
  for (unsigned j=0;j<nj;j++)
  {
    for (unsigned i=0;i<ni;i++)
    {
      buff[numFloats*count] = 0.0f;
      buff[numFloats*count + 1] = 0.0f;
      buff[numFloats*count + 2] = 1.0f;
      buff[numFloats*count + 3] = 0.0f;
      //rgba values
      buff[numFloats*count + 4] = 0.0f;
      buff[numFloats*count + 5] = 0.0f;
      buff[numFloats*count + 6] = 0.0f;
      buff[numFloats*count + 7] = 1.0f;
      if (i<input_image->ni() && j< input_image->nj()) {
        vil_rgba<vxl_byte> rgba = (*input_image)(i,j); 
        buff[numFloats*count + 4] = (float) rgba.R()/255.0f;
        buff[numFloats*count + 5] = (float) rgba.G()/255.0f;
        buff[numFloats*count + 6] = (float) rgba.B()/255.0f;
        buff[numFloats*count + 7] = (float) rgba.A()/255.0f;
      }
      ++count;
    }
  }
  //now write to bocl_mem
  if (!image_) {
    //create mem
    image_ = new bocl_mem((*context_), buff, ni*nj * sizeof(cl_float8), "input image buffer");
    image_->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
  }
  else {
    image_->write_to_buffer(*command_queue_);
  }
  return true;
}