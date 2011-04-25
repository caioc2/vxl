#ifndef boxm2_render_functions_h_
#define boxm2_render_functions_h_

// Render block functions (make use of the render functor classes)
//
#include "boxm2_render_exp_image_functor.h"

void boxm2_render_expected_image( boxm2_scene_info * linfo,
                                  boxm2_block * blk_sptr,
                                  vcl_vector<boxm2_data_base*> & datas,
                                  vpgl_camera_double_sptr cam ,
                                  vil_image_view<float> *expected,
                                  vil_image_view<float> * vis,
                                  unsigned int roi_ni,
                                  unsigned int roi_nj,
                                  unsigned int roi_ni0=0,
                                  unsigned int roi_nj0=0); 


void boxm2_render_cone_exp_image(boxm2_scene_info * linfo,
                                boxm2_block * blk_sptr,
                                vcl_vector<boxm2_data_base*> & datas,
                                vpgl_camera_double_sptr cam ,
                                vil_image_view<float> *expected,
                                vil_image_view<float> * vis,
                                unsigned int roi_ni,
                                unsigned int roi_nj,
                                unsigned int roi_ni0=0,
                                unsigned int roi_nj0=0); 


#endif  //boxm2_render_functions_h_