#ifndef boxm2_update_functions_h_
#define boxm2_update_functions_h_

// Render block functions (make use of the render functor classes)
//

#include <boxm2/io/boxm2_cache.h>
#include <boxm2/boxm2_scene.h>
#include <vil/vil_image_view.h>

bool boxm2_update_cone_image(boxm2_scene_sptr & scene,
                             vcl_string data_type,
                             vcl_string num_obs_type,
                             vpgl_camera_double_sptr cam ,
                             vil_image_view<float>  * input_img,
                             unsigned int roi_ni,
                             unsigned int roi_nj,
                             unsigned int roi_ni0=0,
                             unsigned int roi_nj0=0);


bool boxm2_update_image(boxm2_scene_sptr & scene,
                             vcl_string data_type,int appTypeSize,
                             vcl_string num_obs_type,
                             vpgl_camera_double_sptr cam ,
                             vil_image_view<float>  * input_img,
                             unsigned int roi_ni,
                             unsigned int roi_nj,
                             unsigned int roi_ni0=0,
                             unsigned int roi_nj0=0);

#endif  //boxm2_update_functions_h_
