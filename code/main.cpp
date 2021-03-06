// Disable Error C4996 that occur when using Boost.Signals2.

#include "stdafx.h"
#include "kinect2_grabber.h"
#include "util.h"
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/io/pcd_io.h>
#include<Windows.h>
#include <iostream>
#include <pcl/point_types.h>
#include <Eigen/Geometry>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/point_cloud_color_handlers.h>
#include <time.h>

using namespace pcl;
using namespace std;

typedef pcl::PointXYZRGB PointType;

bool flag = false;
DWORD WINAPI Fun1Proc(LPVOID lpParameter)
{
	system("pause");
	flag = true;
	cout << "开始进行姿态估算..." << endl;
	return 0;
}

int main(int argc, char* argv[])
{
	HANDLE hThread1 = CreateThread(NULL, 0, Fun1Proc, NULL, 0, NULL);

	// PCL Visualizer
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(
		new pcl::visualization::PCLVisualizer("Point Cloud Viewer"));
	viewer->setCameraPosition(0.0, 0.0, -2.5, 0.0, 0.0, 0.0);

	// Point Cloud
	pcl::PointCloud<PointType>::ConstPtr cloud;

	// Retrieved Point Cloud Callback Function
	boost::mutex mutex;
	boost::function<void(const pcl::PointCloud<PointType>::ConstPtr&)> function =
		[&cloud, &mutex](const pcl::PointCloud<PointType>::ConstPtr& ptr) {
		boost::mutex::scoped_lock lock(mutex);

		/* Point Cloud Processing */

		cloud = ptr->makeShared();
	};

	// Kinect2Grabber
	boost::shared_ptr<pcl::Grabber> grabber = boost::make_shared<pcl::Kinect2Grabber>();

	// Register Callback Function
	boost::signals2::connection connection = grabber->registerCallback(function);

	// Start Grabber
	grabber->start();
	while (!viewer->wasStopped()) {
		// Update Viewer
		viewer->spinOnce();
		viewer->removeAllCoordinateSystems();
		viewer->addCoordinateSystem(0.2);
		boost::mutex::scoped_try_lock lock(mutex);
		if (lock.owns_lock() && cloud) {
			// Update Point Cloud
			if (!viewer->updatePointCloud(cloud, "cloud")) {
				viewer->addPointCloud(cloud, "cloud");
				//pcl::io::savePCDFile("cloud.pcd", *cloud);
				//cout << "save cloud.pcd file done." << endl;
			}
			if (flag) {
				cout << "初始化..." << endl;
				/*pose estimation*/
				clock_t start, finish1, finish2, finish3, finish4, finish;
				start = clock();
				////////euclid seg/////////
				std::vector< pcl::PointCloud<pcl::PointXYZRGB>::Ptr > sourceClouds;
				pcl::PointCloud<PointType>::Ptr cloud2(new pcl::PointCloud<pcl::PointXYZRGB>);
				*cloud2 = *cloud;
				//pcl::io::savePCDFile("cloud.pcd", *cloud);
				//cout << "save cloud.pcd file done." << endl;
				
				try {
					FFilter(cloud2, cloud2);
				}
				catch(double d){
					continue;
				}
				Euclid_Seg(cloud2, &sourceClouds);
				cout << "euclid seg done" << endl;
				finish1 = clock();

				////////choose the highest score/////
				double max_score = 0;
				int max_index = 0;
				//cout<<sourceClouds.size();
				if (sourceClouds.size() <= 0 || sourceClouds.size()>10)
					continue;
				for (int i = 0; i < sourceClouds.size(); i++)
				{
					double score;
					pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_input(new pcl::PointCloud<pcl::PointXYZ>);
					pcl::copyPointCloud(*sourceClouds[i], *cloud_input);
					score = Detect(cloud_input);
					if (score > max_score) {
						max_score = score;
						max_index = i;
					}

				}
				//cout<<max_score<<" ";
				cout << "score done" << endl;
				finish2 = clock();

				////////lccp///////
				pcl::PointCloud<pcl::PointXYZRGBA>::Ptr input_lccp_cloud(new pcl::PointCloud<pcl::PointXYZRGBA>);
				pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_final(new pcl::PointCloud<pcl::PointXYZ>);
				pcl::copyPointCloud(*sourceClouds[max_index], *input_lccp_cloud);
				//pcl::io::savePCDFile("final.pcd",*input_lccp_cloud);
				Lccp(input_lccp_cloud, cloud_final);
				double percent = 0.0;
				while (percent < 0.9) {
					percent = Lccp(input_lccp_cloud, cloud_final);
					pcl::copyPointCloud(*cloud_final, *input_lccp_cloud);
				}
				cout << "lccp done" << endl;
				finish3 = clock();

				////////detect plane///////
				pcl::PointCloud<pcl::PointXYZ>::Ptr plane(new pcl::PointCloud<pcl::PointXYZ>);
				double sum_x, sum_y, sum_z;
				Plane(cloud_final, plane, &sum_x, &sum_y, &sum_z);
				//cout<<sum_x<<endl;

				finish4 = clock();

				////////calculate loc&pose///////
				Eigen::Affine3f tt;
				Estimation(plane, &tt, &sum_x, &sum_y, &sum_z);
				cout << tt.matrix() << endl;

				//cout<<tt.matrix()<<endl;
				cout << "calculate done" << endl;
				finish = clock();
				cout << (finish - start) / double(CLOCKS_PER_SEC) << " (s) " << endl;
				cout << 100 * (finish1 - start) / (finish - start) << "% ";
				cout << 100 * (finish2 - finish1) / (finish - start) << "% ";
				cout << 100 * (finish3 - finish2) / (finish - start) << "% ";
				cout << 100 * (finish4 - finish3) / (finish - start) << "% ";
				cout << 100 * (finish - finish4) / (finish - start) << "%" << endl;

				////////visualize//////
				//pcl::visualization::PCLVisualizer viewer3("demo");
				//viewer3.addPointCloud(cloud, "origin");
				pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> cloud_out(plane, 255, 0, 0);
				if(!viewer->updatePointCloud(plane,cloud_out,"plain"))
					viewer->addPointCloud(plane, cloud_out, "plain");
				viewer->updatePointCloud(plane,cloud_out,"plain");
				viewer->addCoordinateSystem(0.5, tt);
				
			}

		}
	}
	// Stop Grabber
	grabber->stop();
	CloseHandle(hThread1);
	// Disconnect Callback Function
	if (connection.connected()) {
		connection.disconnect();
	}

	return 0;
}
