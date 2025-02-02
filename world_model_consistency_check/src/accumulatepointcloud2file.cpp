// Includes
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>

#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl_ros/transforms.h>

// Definitions
typedef pcl::PointXYZRGB PointT;

// Global vars
std::string filename = "/tmp/output.pcd";
pcl::PointCloud<PointT>::Ptr accumulated_cloud;
tf::TransformListener* p_listener;
boost::shared_ptr<ros::Publisher> pub;

float voxel_size=0.05;

void cloud_open_target(const sensor_msgs::PointCloud2ConstPtr& msg)
{
  // declare variables
  pcl::PointCloud<PointT>::Ptr cloud;
  pcl::PointCloud<PointT>::Ptr cloud_transformed;
  pcl::PointCloud<PointT>::Ptr tmp_cloud;
  pcl::VoxelGrid<PointT> grid;
  sensor_msgs::PointCloud2 msg_out;

  // allocate objects for pointers
  cloud = (pcl::PointCloud<PointT>::Ptr)(new pcl::PointCloud<PointT>);
  cloud_transformed = (pcl::PointCloud<PointT>::Ptr)(new pcl::PointCloud<PointT>);
  tmp_cloud = (pcl::PointCloud<PointT>::Ptr)(new pcl::PointCloud<PointT>);

  // Convert the ros message to pcl point cloud
  pcl::fromROSMsg(*msg, *cloud);

  // Remove any NAN points in the cloud
  std::vector<int> indicesNAN;
  pcl::removeNaNFromPointCloud(*cloud, *cloud, indicesNAN);
  // pcl::removeNaNFromPointCloud(*accumulated_cloud, *accumulated_cloud, indicesNAN);

  // Get the transform, return if cannot get it
  ros::Time tic = ros::Time::now();
  ros::Time t = msg->header.stamp;
  tf::StampedTransform trans;
  try
  {
    p_listener->waitForTransform(ros::names::remap("/map"), msg->header.frame_id, t, ros::Duration(3.0));
    p_listener->lookupTransform(ros::names::remap("/map"), msg->header.frame_id, t, trans);
  }
  catch (tf::TransformException& ex)
  {
    ROS_ERROR("%s", ex.what());
    // ros::Duration(1.0).sleep();
    ROS_WARN("Cannot accumulate");
    return;
  }
  ROS_INFO("Collected transforms (%0.3f secs)", (ros::Time::now() - tic).toSec());

  // Transform point cloud using the transform obtained
  Eigen::Affine3d eigen_trf;
  tf::transformTFToEigen(trans, eigen_trf);
  pcl::transformPointCloud<PointT>(*cloud, *cloud_transformed, eigen_trf);

  // Accumulate the point cloud using the += operator
  ROS_INFO("Size of cloud_transformed = %ld", cloud_transformed->points.size());
  (*accumulated_cloud) += (*cloud_transformed);

  // Voxel grid filter the accumulated cloud
  *tmp_cloud = *accumulated_cloud;
  grid.setInputCloud(tmp_cloud);
  // grid.setLeafSize (0.5f, 0.5f, 0.5f);
  // grid.setLeafSize(0.05f, 0.05f, 0.05f);
  grid.setLeafSize(voxel_size, voxel_size, voxel_size);
  grid.filter(*accumulated_cloud);
  ROS_INFO("Size of accumulated_cloud = %ld", accumulated_cloud->points.size());

  // Conver the pcl point cloud to ros msg and publish
  pcl::toROSMsg(*accumulated_cloud, msg_out);
  msg_out.header.stamp = t;
  pub->publish(msg_out);

  cloud.reset();
  tmp_cloud.reset();
  cloud_transformed.reset();
}

int main(int argc, char** argv)
{
  // Initialize ROS
  ros::init(argc, argv, "accumulatepointcloud");
  ros::NodeHandle nh;

  ros::param::get("~filename", filename);
  ros::param::get("~voxel_size", voxel_size);

  // initialize the transform listenerand wait a bit
  p_listener = (tf::TransformListener*)new tf::TransformListener;
  ros::Duration(2).sleep();

  // Initialize accumulated cloud variable
  accumulated_cloud = (pcl::PointCloud<PointT>::Ptr) new pcl::PointCloud<PointT>;
  accumulated_cloud->header.frame_id = ros::names::remap("/map");

  // Initialize the point cloud publisher
  pub = (boost::shared_ptr<ros::Publisher>)new ros::Publisher;
  *pub = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_point_cloud", 1);

  // Create a ROS subscriber for the input point cloud
  ros::Subscriber sub_target = nh.subscribe("input", 1, cloud_open_target);

  // Loop infinetly
  while (ros::ok())
  {
    // Spin
    ros::spinOnce();
  }

  // Save accumulated point cloud to a file
  printf("Saving to file %s\n", filename.c_str());
  ;
  pcl::io::savePCDFileASCII(filename, *accumulated_cloud);
}
