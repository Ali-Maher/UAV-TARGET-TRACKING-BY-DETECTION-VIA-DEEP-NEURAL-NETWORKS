//
// Created by ali-maher on 17-10-1.
//
#include "../include/ibvs_node.h"
#include <ros/ros.h>
//for image msg
#include <sensor_msgs/Image.h>
//to convert to openCV format
#include <cv_bridge/cv_bridge.h>
//#include "opencv2/core.hpp"
#include <opencv2/highgui/highgui.hpp>
//#include <opencv2/imgproc/imgproc.hpp>
//#include <sensor_msgs/image_encodings.h>
//detector node service output
#include "rail_object_detector/SceneQuery.h"
// to control Bebop
#include <geometry_msgs/Twist.h>





/*class ibvs_controller
{
    //ros::NodeHandle nh_;
    //ros::ServiceClient serviceClient;
    //cv_bridge::CvImagePtr cvImagePtr;
    //rail_object_detector::SceneQuery::Request request;
    rail_object_detector::SceneQuery::Response response;// only response was defined
    //image_transport::ImageTransport it_;
    //image_transport::Subscriber image_sub_;
    //image_transport::Publisher image_pub_;

    geometry_msgs::Twist command;

    ros::Publisher twist_pub_;

    double cam_x;
    double cam_y;
ibvs_controller()
{
  //  serviceClient = nh_.serviceClient<rail_object_detector::SceneQuery>("/detector_node/objects_in_scene");
    serviceClient
}
};*/

static cv::Scalar randomColor(cv::RNG& rng)
{
    int icolor = (unsigned)rng;
    return cv::Scalar(icolor&255, (icolor>>8)&255, (icolor>>16)&255);
}



int main(int argc, char** argv)
{

    printf("Starting Bebop IBVS Controller...\n");

    ros::init(argc, argv, "ibvs_node");// create ibvs node (should be base name)
    ros::NodeHandle nh_;
    ros::ServiceClient serviceClient = nh_.serviceClient<rail_object_detector::SceneQuery>("/detector_node/objects_in_scene");
    ros::Publisher bebop_cmd = nh_.advertise<geometry_msgs::Twist>("bebop/cmd_vel",1);
    geometry_msgs::Twist command;
   // ros::Publisher publisher_sim =nh_.advertise<geometry_msgs::Twist_>("turtl1/cmd_vel",100);
    //ros::ServiceClient serviceClient = nh_.advertise<rail_object_detector::SceneQuery>("/detector_node/objects_in_scene",true);
    rail_object_detector::SceneQuery::Request request;
    rail_object_detector::SceneQuery::Response response;
    //call service once to  get image size "response.image.height"
    serviceClient.call(request,response);
    cv_bridge::CvImagePtr cv_ptr;
    double cam_x=-0;// 
    double cam_y=-0;
    double cam_z=-0;
    cv::namedWindow(IBVS_WINDOW, CV_WINDOW_AUTOSIZE);//
    cv::createTrackbar("Dead_Zone_X",IBVS_WINDOW,&DEAD_ZONE_X,140);//to adjust the Dead_zone online
    cv::createTrackbar("Dead_Zone_Y",IBVS_WINDOW,&DEAD_ZONE_Y,120);
    cv::createTrackbar("Dead_Zone_Scale",IBVS_WINDOW,&DEAD_ZONE_SCALE,200);
    cv::Mat detected_img;
    cv::Point p1, p2;
    int num_of_persons=0;
    int person_index=-1;
    //------for illustration only----
    static const double font_scale = 0.6;
    static const float text_size = 40*font_scale;
    static const int text_thickness = 2;
    static const int first_line_position = 7;
    static const cv::Scalar blue_color( 214,   0,   0);
    static const cv::Scalar green_color(  0, 153,   0);
    static const cv::Scalar red_color(    0,   0, 214);
    static const cv::Scalar orange_color( 0, 102, 204);
    cv::Point p1_line,p2_line,p3_line,target_center;
    p1_line.x = 0;
    p1_line.y = response.image.height - (int)text_size - first_line_position;
    p2_line.x = 0;
    p2_line.y = response.image.height - (int)text_size - first_line_position-20;
    p3_line.x = 0;
    p3_line.y = response.image.height - (int)text_size - first_line_position-40;
    ros::Rate rate(25);// 25 fps
    while (ros::ok()) {
    //request the response of the objects_in_scene service
  //  bool success =
        serviceClient.call(request,response);
// Check  o r success and use the response .
 /*   if (success) {
        ROS_INFO_STREAM("service responds successfully") ;
    } else {
        ROS_ERROR_STREAM(" F a i l e d ") ;
    }*/

        try {
            cv_ptr = cv_bridge::toCvCopy(response.image, sensor_msgs::image_encodings::BGR8);
            // place holders for incoming original image, that image converted into HSV, and the HSV image filtered by thresholds to create a binary map searching for target color
            detected_img = cv_ptr->image;
            cv::RNG rng(0xFFFFFFFF);
            for (int i = 0; i < response.objects.size(); i++) {
                if (response.objects[i].probability >= 0.4) {
                    p1.x = response.objects[i].left_bot_x;
                    p1.y = response.objects[i].left_bot_y;
                    p2.x = response.objects[i].right_top_x;
                    p2.y = response.objects[i].right_top_y;
                    cv::rectangle(detected_img, p1, p2, randomColor(rng), 4);
                    //ROS_INFO_STREAM("object_name: " << response.objects[i].label << "," << "confidence"
                                           //         << response.objects[i].probability);
                    //if (response.objects[i].label == "cell phone") {// to enter cell phone follower mode when one cell phone only is detected
                    if (response.objects[i].label == "person") {// to enter person follower mode when one person only is detected
                        num_of_persons++;
                        person_index = i;
                        }

                    }
                }

                //int img_width =detected_img.cols;
                //printf("nu of parsons %d",num_of_persons);

                if (num_of_persons == 1) {

                    // 1- correction in horizontal image plan by Yawing"
                    if (response.objects[person_index].centroid_x < (detected_img.cols / 2 - DEAD_ZONE_X)) {// the target's center is in left side
                        if (SHOW_TURN_VALUES)
                            printf("Move left %f", cam_x);
                        cam_x = CAM_SPEED;// CCW +ive command to turn left

                    } else if (response.objects[person_index].centroid_x > (detected_img.cols / 2 + DEAD_ZONE_X)) {// the target's center is in right side
                        if (SHOW_TURN_VALUES)
                            printf("Move right %f", cam_x);
                        cam_x = -CAM_SPEED;//CW -ive command to turn left

                    } else {

                        cam_x = 0; // dead zone
                        cv::putText(detected_img,"Target in deadZone_X",p1_line,cv::FONT_HERSHEY_SIMPLEX,font_scale,red_color,text_thickness);

                    }

                    static double speedup_x = // more faster for far target center.
                            (response.objects[person_index].centroid_x - (detected_img.cols / 2)) * SPEEDUP_FACTOR;// +ive in case of right target
                                                                                                                    //-ve in ase of left target
                    command.angular.z = cam_x - speedup_x;
                    command.angular.x = 0.0;
                    command.angular.y = 0.0;
                    command.linear.x = 0.0;
                    command.linear.y = 0.0;
                    command.linear.z = 0.0;


                    // 2- correction in vertical image plan by dz(fly up/down) (velocity in z direction)
                    if (response.objects[person_index].right_top_y < (detected_img.rows / 2 - DEAD_ZONE_Y)) {// the target's center is in upper side
                        if (SHOW_TURN_VALUES)
                            printf("Move up %f", cam_y);
                        cam_y = CAM_SPEED;//  +ive command to Fly up

                    } else if (response.objects[person_index].right_top_y > (detected_img.rows / 2 )) {// the target's center is in lower side
                        if (SHOW_TURN_VALUES)
                            printf("Move down %f", cam_y);
                        cam_y = -CAM_SPEED; //-ive command to fly down

                    } else {

                        cam_y = 0; // dead zone
                        cv::putText(detected_img,"Target in deadZone_Y",p2_line,cv::FONT_HERSHEY_SIMPLEX,font_scale,green_color,text_thickness);

                    }

                    static double speedup_y = // more faster for far target center.
                            (response.objects[person_index].right_top_y - (detected_img.rows / 2)) * SPEEDUP_FACTOR;// -ive in case of upper target
                                                                                                                  //+ve in ase of lower target
                    //command.linear.z = cam_y - speedup_y;

                    // 3- correction in depth (how target far/close) (fly FWD/BWD) (velocity in x direction)
                    static double bb_area = (double(((response.objects[person_index].right_top_x-response.objects[person_index].left_bot_x)*(response.objects[person_index].left_bot_y-response.objects[person_index].right_top_y)))) / ((double)(response.image.height*response.image.width));
                    if (bb_area  < (DEAD_ZONE_SCALE/100.0-0.1)) {// the target is far -->fly FWD
                        if (SHOW_TURN_VALUES)
                            printf("Move FWD %f", cam_z);
                        cam_z = CAM_SPEED;//  +ive command to fly FWD

                    } else if (bb_area  > (DEAD_ZONE_SCALE/100.0+0.1)) {// the target is close -->Fly BWD
                        if (SHOW_TURN_VALUES)
                            printf("Move BWD %f", cam_z);
                        cam_z = -CAM_SPEED; //-ive command to fly down

                    } else {

                        cam_z = 0; // dead zone
                        cv::putText(detected_img,"Target in deadZone_Z",p3_line,cv::FONT_HERSHEY_SIMPLEX,font_scale,blue_color,text_thickness);

                    }//start from here

                    static double speedup_z = // more faster for far target center.
                            (bb_area ) * SPEEDUP_FACTOR;// -ive in case of upper target
                    //+ve in case of lower target
                   // command.linear.x = cam_z - speedup_z;
                    printf("scale: %f\n",bb_area );


                    //printf("speedup_x:%f speedup_y:%f speedup_z:%f \n", speedup_x,speedup_y,speedup_z);
                   // printf("speedup_x:%f \n", speedup_x);
                    bebop_cmd.publish(command);//just yawing command


                }

                num_of_persons = 0;//reset
                //after  drawing BB and
                cv::imshow(IBVS_WINDOW, detected_img);
                cv::waitKey(1);



            }




        catch (cv_bridge::Exception& e)
        {
            ROS_ERROR("cv_bridge exception: %s", e.what());
            return 0;
        }

        rate.sleep();
       // ROS_INFO_STREAM( " successfully");
}
// ibvs_controller ibvs;
 //   ros::spin();
    return 0;
}
