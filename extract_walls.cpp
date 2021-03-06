//
// Created by czh on 10/17/18.
//
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <vector>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/io/vtk_io.h>
#include <pcl/search/search.h>
#include <pcl/search/kdtree.h>
#include <pcl/features/normal_3d.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/segmentation/sac_segmentation.h> //RANSACのため
#include <cmath>
#include <pcl/filters/project_inliers.h> //平面に投影するため
#include <pcl/filters/passthrough.h>
#include <pcl/common/pca.h>
#include <pcl/common/common.h>
#include <pcl/io/obj_io.h> //obj形式で保存するため
#include <pcl/filters/voxel_grid.h> //ダウンサンプリングのため
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/common/geometry.h>
#include <pcl/filters/conditional_removal.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/json.h>
#include "extract_walls.h"
#include "Model.h"
#include "Plane.h"
#include "SimpleView.h"
#include "Reconstruction.h"
#include "DxfExporter.h"
#include <pcl/surface/concave_hull.h>
#include <pcl/surface/convex_hull.h>
#include <yaml-cpp/yaml.h>

#include <pcl/console/print.h>
using namespace std;
using namespace pcl;
typedef pcl::PointXYZRGB PointRGB;
typedef pcl::PointXYZRGBNormal PointT;
typedef pcl::PointCloud<PointT> PointCloudT;

YAML::Node config ;

template<typename T>
T Paras(string a)
{
    return config[a].as<T>();
}

template<typename T>
T Paras(string a, string b)
{
    YAML::Node aa = config[a];
    return aa[b].as<T>();
}

int main(int argc, char** argv) {

	PCL_WARN("This program is based on assumption that ceiling and ground on the X-Y  \n");
	pcl::console::setVerbosityLevel(pcl::console::L_ALWAYS); // for not show the warning
	PointCloudT::Ptr allCloudFilled(new PointCloudT);
	vector<Plane> filledPlanes;
	vector<Plane> horizontalPlanes;
	vector<Plane> upDownPlanes;
	vector<Plane> wallEdgePlanes;
	#ifdef _WIN32
		//string fileName = argv[2];
		string fileName = "TestData/Room_A.ply";
	#elif defined __unix__
		string fileName = "";
		string configPath = "../config.yaml";
		if (argc == 3) {
		    fileName = argv[1];
            configPath = argv[2];
		}else{
            cout << "1. RoomA \n";
		    cout << "2. RoomE\n";
            cout << "3. Hall\n";
		    int index = 0;
		    cout << "Please choose test dataset " << " \n";
            cin >> index;
            if (index == 1) fileName = "/home/czh/Desktop/PointCloudDataset/LaserScanner/old/Room_A.ply";
            else if (index == 2) fileName = "/home/czh/Desktop/PointCloudDataset/LaserScanner/roomE_empty/roomE_empty.ply";
            else if (index == 3) fileName = "/home/czh/Desktop/PointCloudDataset/LaserScanner/cooridor/Downsampled_coorridor_all.ply";
        }
    #endif
    config = YAML::LoadFile(configPath);
	Reconstruction re(fileName);
	if (Paras<bool>("View","original")) simpleView("input original point clouds", re.pointCloud);

	smoothNoise(re.pointCloud, Paras<int>("SmoothNoise","K"), Paras<int>("SmoothNoise","beta"));
	if (Paras<bool>("View","smoothNoise")) simpleView("[smooth Noise]", re.pointCloud);

    densityFilter(re.pointCloud);
	if (Paras<bool>("View","densityFilter")) simpleView("[densityFilter]", re.pointCloud);

	float step = 1 / Paras<int>("pointPitch");
	PointCloudT::Ptr twoDimPts(new PointCloudT);

    float heightLow,heightHigh;
    detectHeightRange(re.pointCloud,heightHigh, heightLow);

	convert2D(re.pointCloud,twoDimPts);
	if (Paras<bool>("View","2D")) simpleView("[convert2D]", twoDimPts);


    PointCloudT::Ptr largestComp(new PointCloudT);
    findBiggestComponent2D(twoDimPts, largestComp);
	if (Paras<bool>("View","BiggestComponent")) simpleView("[findBiggestComponent2D]", largestComp);


    PointCloudT::Ptr hullOutput(new PointCloudT);
    computeHull(largestComp, hullOutput, Paras<float>("ComputeHull","alpha"));

	if (Paras<bool>("View","hull")) simpleView("[compute hull] ", hullOutput);

    // extract lines from edge point clouds
    vector<EdgeLine> edgeLines;
    extractLineFromEdge(hullOutput, edgeLines);
	if (Paras<bool>("View","extractLine")) simpleView("[extractLineFromEdge] result" , edgeLines);


	// extract lines from edge point clouds
	vector<EdgeLine> edgeLines2;
	extractLineFromEdge2(hullOutput, edgeLines2);
	if (Paras<bool>("View","extractLine")) simpleView("[extractLineFromEdge2] result " , edgeLines2);


	//if (Paras<bool>("BeamRecons","isExist")) applyBeamExtraction(re.pointCloud, heightHigh, edgeLines);

	if (Paras<bool>("BeamRecons","isExist")) BeamRANSAC(re.pointCloud,heightHigh);

	findLinkedLines(edgeLines2);
	cout << "after find linked lines, extract " << edgeLines.size() << " lines\n";
	if (Paras<bool>("View","linkedEdges")) simpleView("line pts after findLinkedLines" , edgeLines2);

	exportToDxf("./output.dxf", edgeLines2, heightLow, heightHigh);
    return 0;
/*    PointT min, max;
    pcl::getMinMax3D(*topTemp,min,max);
    simpleView("topTemp ", topTemp);
    PointCloudT::Ptr roofEdgePts(new PointCloudT);
    for (float i = min.x; i < max.x; i += step) { // NOLINT
        // extract x within [i,i+step] -> tempX
        PointCloudT::Ptr topTempX(new PointCloudT);
        pcl::PassThrough<PointT> filterX;
        filterX.setInputCloud(topTemp);
        filterX.setFilterFieldName("x");
        filterX.setFilterLimits(i, i + 0.1);
        filterX.filter(*topTempX);
        // found the minY and maxY
        PointT topTempY_min, topTempY_max;
        pcl::getMinMax3D(*topTempX, topTempY_min, topTempY_max);
        PointT p1, g1;
        p1.x = i; p1.y = topTempY_min.y; p1.z = 0;
        g1.x = i; g1.y = topTempY_max.y; g1.z = 0;
        g1.r = 255;
        p1.r = 255;
        if (abs(p1.y) < 10000 && abs(p1.z) < 10000 && abs(g1.y) < 10000 && abs(g1.z) < 10000) {
            roofEdgePts->push_back(p1);
            roofEdgePts->push_back(g1);
        }
    }
	for (float i = min.y; i < max.y; i += step) { // NOLINT
		// extract x within [i,i+step] -> tempX
		PointCloudT::Ptr topTempX(new PointCloudT);
		pcl::PassThrough<PointT> filterX;
		filterX.setInputCloud(topTemp);
		filterX.setFilterFieldName("y");
		filterX.setFilterLimits(i, i + 0.1);
		filterX.filter(*topTempX);
		// found the minY and maxY
		PointT topTempY_min, topTempY_max;
		pcl::getMinMax3D(*topTempX, topTempY_min, topTempY_max);
		PointT p1, g1;
		p1.y = i; p1.x = topTempY_min.x; p1.z = 0;
		g1.y = i; g1.x = topTempY_max.x; g1.z = 0;
		g1.r = 255;
		p1.r = 255;
		if (abs(p1.x) < 10000 && abs(p1.z) < 10000 && abs(g1.x) < 10000 && abs(g1.z) < 10000) {
			roofEdgePts->push_back(p1);
			roofEdgePts->push_back(g1);
		}
	}
    cout << "num " << roofEdgePts->size() << endl;
    simpleView("roof Edge", roofEdgePts);

	vector<PointCloudT::Ptr> roofEdgeClusters;
	vector<Eigen::VectorXf> roofEdgeClustersCoffs;
	for (int m = 0; m < 10; ++m) {
		if(roofEdgePts->size() == 0) break;
		pcl::ModelCoefficients::Ptr sacCoefficients(new pcl::ModelCoefficients);
		pcl::PointIndices::Ptr sacInliers(new pcl::PointIndices);
		pcl::SACSegmentation<PointT> seg;
		seg.setOptimizeCoefficients(true);
		seg.setModelType(pcl::SACMODEL_LINE);
		seg.setMethodType(pcl::SAC_RANSAC);
		seg.setDistanceThreshold(0.05);
		seg.setInputCloud(roofEdgePts);
		seg.segment(*sacInliers, *sacCoefficients);
		Eigen::VectorXf coff(6);
		coff << sacCoefficients->values[0], sacCoefficients->values[1], sacCoefficients->values[2],
							 sacCoefficients->values[3], sacCoefficients->values[4], sacCoefficients->values[5];

		PointCloudT::Ptr extracted_cloud(new PointCloudT);
		pcl::ExtractIndices<PointT> extract;
		extract.setInputCloud(roofEdgePts);
		extract.setIndices(sacInliers);
		extract.setNegative(false);
		extract.filter(*extracted_cloud);
		Reconstruction tmpRe(extracted_cloud);
		tmpRe.applyRegionGrow(paras.roof_NumberOfNeighbours, paras.roof_SmoothnessThreshold,
								paras.roof_CurvatureThreshold, paras.roof_MinSizeOfCluster, paras.KSearch);
		vector<PointCloudT::Ptr> clusters = tmpRe.clusters;
		for(auto &c:clusters) {
			roofEdgeClusters.push_back(c);
			roofEdgeClustersCoffs.push_back(coff);
		}
		extract.setNegative(true);
		extract.filter(*roofEdgePts);
		cout << "roofEdgePts size " << roofEdgePts->size() << endl;
	}



	vector<Eigen::Vector3i> colors;
	for (int k = 0; k < roofEdgeClusters.size(); ++k) {
		colors.push_back(Eigen::Vector3i(255,0,0)); // red
		colors.push_back(Eigen::Vector3i(255,255,0)); // yellow
		colors.push_back(Eigen::Vector3i(0,0,255)); // blue
		colors.push_back(Eigen::Vector3i(0,255,0)); // green
		colors.push_back(Eigen::Vector3i(0,255,255)); // cyan
		colors.push_back(Eigen::Vector3i(255,0,255)); // pink
	}
	PointCloudT::Ptr roofClusterPts(new PointCloudT);
	for (int l = 0; l < roofEdgeClusters.size(); ++l) {
		int color = 255 <<24 | colors[l][0] << 16 | colors[l][1] << 8 | colors[l][2];
		for (auto &p:roofEdgeClusters[l]->points) {
			p.rgba = color;
			roofClusterPts->push_back(p);
		}
	}
	cout << "roof segmentation: size of clusters: " << roofEdgeClusters.size() << endl;
	simpleView("roof Edge Segmentation", roofClusterPts);

	// mark: filter the height based on z values of upDown Planes
	cout << "\nHeight Filter: point lower than " << ZLimits[0] << " and higher than " << ZLimits[1] << endl;
	for (auto &filledPlane : filledPlanes) {
		filledPlane.applyFilter("z", ZLimits[0], ZLimits[1]);
	}


	// Mark: we control the distance between two edges and the height difference between two edges

	for (int i = 0; i < filledPlanes.size(); ++i) {
		Plane* plane_s = &filledPlanes[i];
		for (int j = i + 1; j < filledPlanes.size(); ++j) {
			Plane* plane_t = &filledPlanes[j];
			// mark: combine right -> left
			if (pcl::geometry::distance(plane_s->rightDown(), plane_t->leftDown()) < paras.minimumEdgeDist
				&& pcl::geometry::distance(plane_s->rightUp(), plane_t->leftUp()) < paras.minimumEdgeDist) {
				// mark: if the difference of edge too large, skip
				if (plane_s->getEdgeLength(EdgeRight) - plane_t->getEdgeLength(EdgeLeft) > paras.minHeightDiff) continue;
				Plane filled(plane_s->rightDown(), plane_s->rightUp(), plane_t->leftDown(), plane_t->leftUp(), paras.pointPitch, Color_Red);
				wallEdgePlanes.push_back(filled);
				plane_s->setType(PlaneType_MainWall); plane_t->setType(PlaneType::PlaneType_MainWall);
				// mark: combine left -> right
			}
			else if (pcl::geometry::distance(plane_s->leftDown(), plane_t->rightDown()) < paras.minimumEdgeDist
				&& pcl::geometry::distance(plane_s->leftUp(), plane_t->rightUp()) < paras.minimumEdgeDist) {

				if (plane_s->getEdgeLength(EdgeLeft) - plane_t->getEdgeLength(EdgeRight) > paras.minHeightDiff) continue;
				plane_s->setType(PlaneType_MainWall); plane_t->setType(PlaneType_MainWall);
				Plane filled(plane_s->leftDown(), plane_s->leftUp(), plane_t->rightDown(), plane_t->rightUp(), paras.pointPitch, Color_Red);
				wallEdgePlanes.push_back(filled);
				// mark: combine left -> left
			}
			else if (pcl::geometry::distance(plane_s->leftDown(), plane_t->leftDown()) < paras.minimumEdgeDist
				&& pcl::geometry::distance(plane_s->leftUp(), plane_t->leftUp()) < paras.minimumEdgeDist) {

				if (plane_s->getEdgeLength(EdgeLeft) - plane_t->getEdgeLength(EdgeLeft) > paras.minHeightDiff) continue;
				plane_s->setType(PlaneType_MainWall); plane_t->setType(PlaneType_MainWall);
				Plane filled(plane_s->leftDown(), plane_s->leftUp(), plane_t->leftDown(), plane_t->leftUp(), paras.pointPitch, Color_Red);
				wallEdgePlanes.push_back(filled);
				// mark: combine right -> right
			}
			else if (pcl::geometry::distance(plane_s->rightDown(), plane_t->rightDown()) < paras.minimumEdgeDist
				&& pcl::geometry::distance(plane_s->rightUp(), plane_t->rightUp()) < paras.minimumEdgeDist) {

				if (plane_s->getEdgeLength(EdgeLeft) - plane_t->getEdgeLength(EdgeLeft) > paras.minHeightDiff) continue;
				plane_s->setType(PlaneType_MainWall); plane_t->setType(PlaneType_MainWall);
				Plane filled(plane_s->rightDown(), plane_s->rightUp(), plane_t->rightDown(), plane_t->rightUp(), paras.pointPitch, Color_Red);
				wallEdgePlanes.push_back(filled);
			}
		}
	}

	// mark: extend main wall planes to z limits
	for (auto &filledPlane : filledPlanes) {
		Plane* plane = &filledPlane;
		if (plane->type() != PlaneType_MainWall) continue;
		PointT leftUp, rightUp, leftDown, rightDown;
		leftUp = plane->leftUp();    leftUp.z = ZLimits[1];
		rightUp = plane->rightUp();   rightUp.z = ZLimits[1];
		leftDown = plane->leftDown();  leftDown.z = ZLimits[0];
		rightDown = plane->rightDown(); rightDown.z = ZLimits[0];
		plane->extendPlane(leftUp, rightUp, plane->leftUp(), plane->rightUp(), paras.pointPitch);
		plane->extendPlane(leftDown, rightDown, plane->leftDown(), plane->rightDown(), paras.pointPitch);
	}

	// mark: found outer planes for inner planes
	for (size_t i = 0; i < filledPlanes.size(); i++) {
		if (filledPlanes[i].type() != PlaneType_Other) continue;
		Plane* source = &filledPlanes[i];
		Eigen::Vector3d s_normal = source->getNormal();// calculatePlaneNormal(*source);
		double s_slope = s_normal[1] / s_normal[0];
		double s_b1 = source->leftUp().y - s_slope * source->leftUp().x;
		double s_b2 = source->rightUp().y - s_slope * source->rightUp().x;
		vector<Plane*> passedPlanes;
		for (size_t j = 0; j < filledPlanes.size(); j++) {
			Plane* target = &filledPlanes[j];
			// mark: First, ingore calculate with self and calculate witl other non-main wall parts
			if (i == j || target->type() == PlaneType_Other) continue;
			Eigen::Vector3d t_normal = target->getNormal();//calculatePlaneNormal(*target);
			// mark: Second, source plane should be inside in target plane (y-z,x-z plane)
			if (target->leftUp().z < source->leftUp().z ||
				target->rightUp().z < source->rightUp().z ||
				target->leftDown().z > source->leftDown().z ||
				target->rightDown().z > source->rightDown().z) {
				continue;
			}

			// mark: Third, angle of two plane should smaller than minAngle
			double angle = acos(s_normal.dot(t_normal) / (s_normal.norm()*t_normal.norm())) * 180 / M_PI;
			if (angle > paras.minAngle_normalDiff) continue;

			// TODO: in x-y plane, whether source plane could be covered by target plane
			bool cond1_p1 = (target->leftUp().y <= (target->leftUp().x * s_slope + s_b1)) && (target->leftUp().y <= (target->leftUp().x * s_slope + s_b2));
			bool cond1_p2 = (target->rightUp().y >= (target->rightUp().x * s_slope + s_b1)) && (target->rightUp().y >= (target->rightUp().x * s_slope + s_b2));

			bool cond2_p1 = (target->leftUp().y >= (target->leftUp().x * s_slope + s_b1)) && (target->leftUp().y >= (target->leftUp().x * s_slope + s_b2));
			bool cond2_p2 = (target->rightUp().y <= (target->rightUp().x * s_slope + s_b1)) && (target->rightUp().y <= (target->rightUp().x * s_slope + s_b2));

			if ((cond1_p1 && cond1_p2) || (cond2_p1 && cond2_p2)) {
				passedPlanes.push_back(target);
			}
		}

		// mark: if passedPlanes exceed 1, we choose the nearest one.
		// fixme: the center may not correct since we assume it is a perfect rectangular
		if (!passedPlanes.empty()) {
			float minDist = INT_MAX;
			size_t minDist_index = 0;
			PointT s_center;
			s_center.x = (source->rightUp().x + source->leftUp().x) / 2;
			s_center.y = (source->rightUp().y + source->leftUp().y) / 2;
			s_center.y = (source->rightUp().z + source->rightDown().z) / 2;
			for (size_t k = 0; k < passedPlanes.size(); ++k) {
				PointT t_center;
				t_center.x = (passedPlanes[k]->rightUp().x + passedPlanes[k]->leftUp().x) / 2;
				t_center.y = (passedPlanes[k]->rightUp().y + passedPlanes[k]->leftUp().y) / 2;
				t_center.y = (passedPlanes[k]->rightUp().z + passedPlanes[k]->rightDown().z) / 2;
				if (pcl::geometry::distance(s_center, t_center) < minDist) {
					minDist = pcl::geometry::distance(s_center, t_center);
					minDist_index = k;
				}
			}
			source->coveredPlane = passedPlanes[minDist_index];
		}
	}

	// mark: since we found the covered planes, we next extend these smaller planes to their covered planes.
	for (auto &filledPlane : filledPlanes) {
		Plane* plane = &filledPlane;
		if (plane->coveredPlane == nullptr) continue;
		plane->setColor(innerPlaneColor);
		// mark: find the projection of plane to its covered plane -> pink
		extendSmallPlaneToBigPlane(*plane, *plane->coveredPlane, 4294951115, paras.pointPitch, allCloudFilled);
	}

	// for final all visualization
	for (auto &filledPlane : filledPlanes) {
		for (size_t j = 0; j < filledPlane.pointCloud->points.size(); j++) {
			allCloudFilled->points.push_back(filledPlane.pointCloud->points[j]);
		}
	}

	for (auto &wallEdgePlane : wallEdgePlanes) {
		for (size_t j = 0; j < wallEdgePlane.pointCloud->points.size(); j++) {
			allCloudFilled->points.push_back(wallEdgePlane.pointCloud->points[j]);
		}
	}

	// fill the ceiling and ground
	{
		PointT min, max;
		float step = 1 / (float)paras.pointPitch;
		pcl::getMinMax3D(*allCloudFilled, min, max);
		PointCloudT::Ptr topTemp(new PointCloudT);
		PointCloudT::Ptr downTemp(new PointCloudT);
		pcl::copyPointCloud(*allCloudFilled, *topTemp);
		pcl::copyPointCloud(*allCloudFilled, *downTemp);
		pcl::PassThrough<PointT> filterZ;
		filterZ.setInputCloud(topTemp);
		filterZ.setFilterFieldName("z");
		filterZ.setFilterLimits(max.z - 2 * step, max.z);
		filterZ.filter(*topTemp);
		filterZ.setInputCloud(downTemp);
		filterZ.setFilterFieldName("z");
		filterZ.setFilterLimits(min.z, min.z + 2 * step);
		filterZ.filter(*downTemp);

		for (float i = min.x; i < max.x; i += step) { // NOLINT
			// extract x within [i,i+step] -> tempX
			PointCloudT::Ptr topTempX(new PointCloudT);
			PointCloudT::Ptr downTempX(new PointCloudT);
			pcl::PassThrough<PointT> filterX;
			filterX.setInputCloud(topTemp);
			filterX.setFilterFieldName("x");
			filterX.setFilterLimits(i, i + 0.1);
			filterX.filter(*topTempX);
			filterX.setInputCloud(downTemp);
			filterX.filter(*downTempX);
			// found the minY and maxY
			PointT topTempY_min, topTempY_max;
			PointT downTempY_min, downTempY_max;
			pcl::getMinMax3D(*topTempX, topTempY_min, topTempY_max);
			pcl::getMinMax3D(*downTempX, downTempY_min, downTempY_max);

			PointT p1, g1, p2, g2;
			p1.x = i; p1.y = topTempY_min.y; p1.z = topTempY_max.z;
			g1.x = i; g1.y = topTempY_max.y; g1.z = topTempY_max.z;
			p2.x = i; p2.y = downTempY_min.y; p2.z = downTempY_min.z;
			g2.x = i; g2.y = downTempY_max.y; g2.z = downTempY_min.z;
			if (abs(p1.y) < 10000 && abs(p1.z) < 10000 && abs(g1.y) < 10000 && abs(g1.z) < 10000) {
				generateLinePointCloud(p1, g1, paras.pointPitch, 0 << 24 | 255, allCloudFilled);
			}
			if (abs(p2.y) < 10000 && abs(p2.z) < 10000 && abs(g2.y) < 10000 && abs(g2.z) < 10000) {
				generateLinePointCloud(p2, g2, paras.pointPitch, 255 << 24 | 255, allCloudFilled);
			}
		}
	}


	pcl::io::savePLYFile("OutputData/6_AllPlanes.ply", *allCloudFilled);
	simpleView("cloud Filled", allCloudFilled);
	return (0);*/
}


void generateLinePointCloud(PointT pt1, PointT pt2, int pointPitch, int color, PointCloudT::Ptr output) {
	int numPoints = pcl::geometry::distance(pt1, pt2) * pointPitch;
	float ratioX = (pt1.x - pt2.x) / numPoints;
	float ratioY = (pt1.y - pt2.y) / numPoints;
	float ratioZ = (pt1.z - pt2.z) / numPoints;
	for (size_t i = 0; i < numPoints; i++) {

		PointT p;
		p.x = pt2.x + i * (ratioX);
		p.y = pt2.y + i * (ratioY);
		p.z = pt2.z + i * (ratioZ);
		p.rgba = color;
		output->points.push_back(p);
	}
}

void generateLinePointCloud(PointT a, PointT b, PointT c, PointT d, int pointPitch, int color, PointCloudT::Ptr output){
	generateLinePointCloud(a,b,pointPitch, color, output);
	generateLinePointCloud(b,c,pointPitch, color, output);
	generateLinePointCloud(c,d,pointPitch, color, output);
	generateLinePointCloud(d,a,pointPitch, color, output);
}

void extendSmallPlaneToBigPlane(Plane& sourceP, Plane& targetP, int color, int pointPitch, PointCloudT::Ptr output) {
	Eigen::Vector3d normal = sourceP.getNormal();
	float slope = normal[1] / normal[0];
	float b1 = sourceP.leftUp().y - slope * sourceP.leftUp().x;
	float b2 = sourceP.rightUp().y - slope * sourceP.rightUp().x;
	float covered_slope = (targetP.rightUp().y - targetP.leftUp().y) / (targetP.rightUp().x - targetP.leftUp().x);
	float covered_b = targetP.rightUp().y - covered_slope * targetP.rightUp().x;
	Eigen::Matrix2f A;
	A << slope, -1, covered_slope, -1;
	Eigen::Vector2f B1, B2;
	B1 << -b1, -covered_b;
	B2 << -b2, -covered_b;
	Eigen::Vector2f X1, X2;
	X1 = A.colPivHouseholderQr().solve(B1);
	X2 = A.colPivHouseholderQr().solve(B2);
	PointT p1, q1, p2, q2; // four points at covered plane
	p1.x = X1[0]; p1.y = X1[1]; p1.z = sourceP.leftUp().z;
	q1.x = X1[0]; q1.y = X1[1]; q1.z = sourceP.leftDown().z;

	// mark: we need to make sure  X1-left and X2-right wont intersect
	if (isIntersect(p1, q1, sourceP.leftUp(), sourceP.leftDown())) swap(X1, X2);
	p1.x = X1[0]; p1.y = X1[1]; p1.z = sourceP.leftUp().z;
	q1.x = X1[0]; q1.y = X1[1]; q1.z = sourceP.leftDown().z;
	p2.x = X2[0]; p2.y = X2[1]; p2.z = sourceP.leftUp().z;
	q2.x = X2[0]; q2.y = X2[1]; q2.z = sourceP.leftDown().z;

	Plane all;
	Plane tmp_a(p1, q1, sourceP.leftUp(), sourceP.leftDown(), pointPitch, Color_Peach);
	Plane tmp_b(p2, q2, sourceP.rightUp(), sourceP.rightDown(), pointPitch, Color_Peach);
	Plane tmp_c(p1, p2, sourceP.leftUp(), sourceP.rightUp(), pointPitch, Color_Peach);
	Plane tmp_d(q1, q2, sourceP.leftDown(), sourceP.rightDown(), pointPitch, Color_Peach);
	sourceP.append(tmp_a); sourceP.append(tmp_b); sourceP.append(tmp_c); sourceP.append(tmp_d);

	// mark: remove the point which located within four points
	if (X1[0] > X2[0]) swap(X1[0], X2[0]);
	if (X1[1] > X2[1]) swap(X1[1], X2[1]);
	float zMin = sourceP.leftDown().z, zMax = sourceP.leftUp().z;
	float xMin = X1[0], xMax = X2[0];
	float yMin = X1[1], yMax = X2[1];
	targetP.removePointWithin(xMin, xMax, yMin, yMax, zMin, zMax);
}

// fixme: these insect are baed on 2 dimention - fix them in 3d dimention
bool onSegment(PointT p, PointT q, PointT r)
{
	// Given three colinear points p, q, r, the function checks if
	// point q lies on line segment 'pr'
	if (q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) &&
		q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y) &&
		q.z <= max(p.z, r.z) && q.z >= min(p.z, r.z))
		return true;
	return false;
}

float orientation(PointT p, PointT q, PointT r) {
	// To find orientation of ordered triplet (p, q, r).
	// The function returns following values
	// 0 --> p, q and r are colinear
	// 1 --> Clockwise
	// 2 --> Counterclockwise
	float val = (q.y - p.y) * (r.x - q.x) -
		(q.x - p.x) * (r.y - q.y);

	if (val == 0) return 0;  // colinear

	return (val > 0) ? 1 : 2; // clock or counterclock wise
}

bool isIntersect(PointT p1, PointT q1, PointT p2, PointT q2)
{
	// Find the four orientations needed for general and
	// special cases
	float o1 = orientation(p1, q1, p2);
	float o2 = orientation(p1, q1, q2);
	float o3 = orientation(p2, q2, p1);
	float o4 = orientation(p2, q2, q1);

	// General case
	if (o1 != o2 && o3 != o4)
		return true;

	// Special Cases
	// p1, q1 and p2 are colinear and p2 lies on segment p1q1
	if (o1 == 0 && onSegment(p1, p2, q1)) return true;

	// p1, q1 and q2 are colinear and q2 lies on segment p1q1
	if (o2 == 0 && onSegment(p1, q2, q1)) return true;

	// p2, q2 and p1 are colinear and p1 lies on segment p2q2
	if (o3 == 0 && onSegment(p2, p1, q2)) return true;

	// p2, q2 and q1 are colinear and q1 lies on segment p2q2
	if (o4 == 0 && onSegment(p2, q1, q2)) return true;

	return false; // Doesn't fall in any of the above cases
}

void smoothNoise(PointCloudT::Ptr input, int K, float beta) {
    // smooth the noise, filtered points won't found their near K points since they are assume noise,
    // I don't know whether it is right since the paper didn't mention that
    int n = input->size();
    pcl::KdTreeFLANN<PointT> kdtree;
    kdtree.setInputCloud (input);
    unordered_set<int> outliers;
    for (int i = 0; i < n; ++i) {
        //if (outliers.count(i)) continue;  // if points are noise, we ignore
        std::vector<int> pointIdxNKNSearch(K);
        std::vector<float> pointNKNSquaredDistance(K);
        if ( kdtree.nearestKSearch (input->points[i], K, pointIdxNKNSearch, pointNKNSquaredDistance) > 0 )
        {
        	for (auto&value : pointNKNSquaredDistance) value = sqrt(value);
            float mean, stdev;
            calculateMeanStandardDev(pointNKNSquaredDistance, mean, stdev);
            for (int i = 0; i < pointNKNSquaredDistance.size(); ++i) {
            	if (pointNKNSquaredDistance[i] > (mean + beta*stdev) ||
            	    pointNKNSquaredDistance[i] < (mean - beta*stdev) ) {
					outliers.insert(pointIdxNKNSearch[i]);
				}
            }

        }
    }
	pcl::PointIndices::Ptr indices(new pcl::PointIndices());
	indices->indices.insert(indices->indices.end(), outliers.begin(), outliers.end());

	PointCloudT::Ptr filtered(new PointCloudT);
	pcl::ExtractIndices<PointT> extract;
	extract.setInputCloud(input);
	extract.setIndices(indices);
	extract.setNegative(true);
	extract.filter(*input);
	cout << "[smooth noise] remove " << indices->indices.size() << " points from " <<n << ". remind " << input->size() << endl;

}

void densityFilter(PointCloudT::Ptr input){
	float alpha2   = Paras<float>("DensityFilter","alpha2");
    float leafSize = Paras<float>("DensityFilter","leafSize");
	int n = input->size();
	PointCloudT::Ptr filtered(new PointCloudT);
	pcl::VoxelGrid<PointT> sor;
	sor.setInputCloud (input);
	sor.setLeafSize (leafSize, leafSize, leafSize);
	sor.filter(*filtered);
	Eigen::Vector3i divisions = sor.getNrDivisions();

	vector<vector<int>> voxels(divisions[0]*divisions[1]*divisions[2], vector<int>());

	for (int i = 0; i < input->points.size(); ++i) {
		auto cord = sor.getGridCoordinates(input->points[i].x, input->points[i].y, input->points[i].z);
		cord = cord - sor.getMinBoxCoordinates();
		voxels[cord[0]*divisions[1]*divisions[2] + cord[1]*divisions[2] + cord[2]].push_back(i);
	}
	vector<float> voxelsNums;
	for (auto &voxel: voxels) {
		if (voxel.size()!=0) voxelsNums.push_back(voxel.size());
	}
	float mean, stdev;
	calculateMeanStandardDev(voxelsNums, mean, stdev);
	cout <<  "[densityFilter] mean " << mean << ", stdev " << stdev << endl;

	PointCloudT::Ptr output(new PointCloudT);
	for (auto &voxel: voxels) {
		if (voxel.size() < (mean - stdev*alpha2)) continue;
		for (auto &idx :  voxel) output->push_back(input->points[idx]);
	}
	copyPointCloud(*output, *input);
	cout << "[density Filter] left " << input->size() << " from " << n << endl;
}


void detectHeightRange(PointCloudT::Ptr input, float& high, float& low){
    unordered_map<float, int> hist;
    for(auto&p : input->points) {
		hist[roundf(p.z * 10) / 10]++;
	}

    int count1 = 0, count2 = 0; // count1 >= count2
    int maxNum = 0;
    for(auto h:hist) {
    	maxNum = max(h.second, maxNum);
		if (h.second > count1) {
			count2 = count1;
			low = high;
			count1 = h.second;
			high = h.first;
		} else if (h.second > count2) {
			count2 = h.second;
			low = h.first;
		}
	}
    // assert height difference is larger than 2 meters
    assert(abs(high-low) >= 2);
    if (high < low) swap(high, low);
    cout << "[detectHeightRange] [" << low << " " << high << "] max num "<< maxNum << "\n";
}

void extractTopPts(PointCloudT::Ptr input, PointCloudT::Ptr output, float highest, float dimension, reconstructParas paras){
	//PointCloudT::Ptr topTemp(new PointCloudT);

	pcl::PassThrough<PointT> filterZ;
	filterZ.setInputCloud(input);
	filterZ.setFilterFieldName("z");

	filterZ.setFilterLimits(highest - 2 * dimension, highest);
	filterZ.filter(*output);

	Reconstruction topTmp_re(output);
	topTmp_re.applyRegionGrow(paras.NumberOfNeighbours, paras.SmoothnessThreshold,
							  paras.CurvatureThreshold, paras.MinSizeOfCluster, paras.KSearch);
	topTmp_re.getClusterPts(output);
}

void convert2D(PointCloudT::Ptr input,PointCloudT::Ptr output) {
    pcl::copyPointCloud(*input, *output);
    for(auto& p : output->points) {
        p.z = 0;
    }

	cout << "[convert2D] points num " << output->size() << endl;
}

void computeHull(PointCloudT::Ptr input, PointCloudT::Ptr output, float alpha){
	pcl::ConcaveHull<pcl::PointXYZ> concave_hull;
	pcl::PointCloud<pcl::PointXYZ>::Ptr tmpInput(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr tmpOutput(new pcl::PointCloud<pcl::PointXYZ>);

	for (auto& p: input->points) {
		pcl::PointXYZ q;
		q.x = p.x;
		q.y = p.y;
		q.z = p.z;
		tmpInput->push_back(q);
	}
	concave_hull.setInputCloud (tmpInput);
	concave_hull.setAlpha (alpha);
	concave_hull.reconstruct (*tmpOutput);
	for (auto& p: tmpOutput->points) {
		pcl::PointXYZRGBNormal q;
		q.x = p.x;
		q.y = p.y;
		q.z = p.z;
		q.rgba = INT32_MAX;
		output->push_back(q);
	}

	cout << "[computeHull]: before " << input->size() << " -> after " << output->size() << "\n";
}

void findBiggestComponent2D(PointCloudT::Ptr input, PointCloudT::Ptr output){
    assert(input->points[0].z == 0);
    PointCloudT::Ptr tmp(new PointCloudT);
    pcl::copyPointCloud(*input, *tmp);
    vector<PointCloudT::Ptr> groups;
    auto start = std::chrono::system_clock::now();
    seperatePtsToGroups(tmp, 0.1, groups);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    cout << "[findBiggestComponent2D] found " << groups.size() << " groups from " << tmp->size() << " pts. ";
    int maxNum = 0;
    int id = -1;
    for (int i = 0; i < groups.size(); ++i) {
        if (groups[i]->points.size() > maxNum) {
            maxNum = groups[i]->points.size();
            id = i;
        }
    }
    cout << "the max size of groups is  " << maxNum << "  ";
	std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    assert(id >= 0);
    for (auto p : groups[id]->points) output->push_back(p);

}

void extractLineFromEdge(PointCloudT::Ptr input, vector<EdgeLine>& edgeLines){
	PointCloudT::Ptr inputTemp(new PointCloudT);
	copyPointCloud(*input, *inputTemp);
	vector<PointCloudT::Ptr> roofEdgeClusters;
	vector<Eigen::VectorXf> roofEdgeClustersCoffs;
	assert(inputTemp->size() > 0);
	int iter = 0;
	while (iter++ < 1000 && inputTemp->size() > 1) {
		pcl::ModelCoefficients::Ptr sacCoefficients(new pcl::ModelCoefficients);
		pcl::PointIndices::Ptr sacInliers(new pcl::PointIndices);
		pcl::SACSegmentation<PointT> seg;
		seg.setOptimizeCoefficients(true);
		seg.setModelType(pcl::SACMODEL_LINE);
		seg.setMethodType(pcl::SAC_RANSAC);
		seg.setDistanceThreshold(0.05);
		seg.setInputCloud(inputTemp);
		seg.segment(*sacInliers, *sacCoefficients);
		Eigen::VectorXf coff(6);
		coff << sacCoefficients->values[0], sacCoefficients->values[1], sacCoefficients->values[2],
				sacCoefficients->values[3], sacCoefficients->values[4], sacCoefficients->values[5];

		PointCloudT::Ptr extracted_cloud(new PointCloudT);
		pcl::ExtractIndices<PointT> extract;
		extract.setInputCloud(inputTemp);
		extract.setIndices(sacInliers);
		extract.setNegative(false);
		extract.filter(*extracted_cloud);

		vector<PointCloudT::Ptr> tmpClusters;
		seperatePtsToGroups(extracted_cloud, 0.5, tmpClusters); // use this method may make several parts as one part since RANSAC agrees
		for (auto &c:tmpClusters) {
			roofEdgeClusters.push_back(c);
			roofEdgeClustersCoffs.push_back(coff);
		}
		extract.setNegative(true);
		extract.filter(*inputTemp);
	}

//	int sum = 0;
//	for (auto &c:roofEdgeClusters) sum += c->size();

	assert(roofEdgeClusters.size() > 0);
	//simpleView("[extractLineFromEdge] clusters",roofEdgeClusters);

	for (int i = 0; i < roofEdgeClusters.size(); ++i) {
		EdgeLine line;
		ptsToLine(roofEdgeClusters[i], roofEdgeClustersCoffs[i], line);
		if (pcl::geometry::distance(line.p,line.q) > 0.1) edgeLines.push_back(line);
	}
	cout << "[extractLineFromEdge] extract " << edgeLines.size() << " lines"<< endl;

}

void extractLineFromEdge2(PointCloudT::Ptr input, vector<EdgeLine>& edgeLines){
	vector<PointCloudT::Ptr> reginGrow2DOutput;
	vector<Eigen::VectorXf> roofEdgeClustersCoffs;
	regionGrow2D(input, reginGrow2DOutput);

	assert(reginGrow2DOutput.size() > 0);
	if (Paras<bool>("View","extractLine")) simpleView("[extractLineFromEdge2] regionGrow2DOutput",reginGrow2DOutput);

	// using new Generate Pts
	for (auto &Pts : reginGrow2DOutput) {
		pcl::ModelCoefficients::Ptr sacCoefficients(new pcl::ModelCoefficients);
		pcl::PointIndices::Ptr sacInliers(new pcl::PointIndices);
		pcl::SACSegmentation<PointT> seg;
		seg.setOptimizeCoefficients(true);
		seg.setModelType(pcl::SACMODEL_LINE);
		seg.setMethodType(pcl::SAC_RANSAC);
		seg.setDistanceThreshold(0.05);
		seg.setInputCloud(Pts);
		seg.segment(*sacInliers, *sacCoefficients);
		Eigen::VectorXf coff(6);
		coff << sacCoefficients->values[0], sacCoefficients->values[1], sacCoefficients->values[2],
				sacCoefficients->values[3], sacCoefficients->values[4], sacCoefficients->values[5];
		roofEdgeClustersCoffs.push_back(coff);
	}

	for (int i = 0; i < reginGrow2DOutput.size(); ++i) {
		EdgeLine line;
		ptsToLine(reginGrow2DOutput[i], roofEdgeClustersCoffs[i], line);
		if (pcl::geometry::distance(line.p,line.q) > 0.1) edgeLines.push_back(line);
	}

	cout << "[extractLineFromEdge2] extract " << edgeLines.size() << " lines"<< endl;

}

void seperatePtsToGroups(PointCloudT::Ptr input, float radius, vector<PointCloudT::Ptr>& output){

	//TODO this parts can only return indices instead of points, which saves more memory
	assert(input->size() > 0);
    pcl::KdTreeFLANN<PointT> kdtree;
    kdtree.setInputCloud(input);
    unordered_set<int> found;
    int k = 0;
	while(found.size() < input->size()) {
	    // cout << "found size " << found.size() << "\n";
        vector<int> group;
        stack<PointT> seeds;

        while(found.count(k)) k++;
        if (k >= input->size()) break;
        seeds.push(input->points[k]);
        found.insert(k);
        group.push_back(k);


        while(!seeds.empty()) {
            PointT seed = seeds.top();
            seeds.pop();
            vector<int> pointIdx;
            vector<float> dist;
            kdtree.radiusSearch(seed, radius, pointIdx,dist);
			for(auto& ix : pointIdx) {
                if (!found.count(ix))  {
                    found.insert(ix);
                    seeds.push(input->points[ix]);
                    group.push_back(ix);
                }
            }
        }
        PointCloudT::Ptr foundPts(new PointCloudT);
        for (auto& idx:group) {
            foundPts->points.push_back(input->points[idx]);
        }
        output.push_back(foundPts);
	}
}

void ptsToLine(PointCloudT::Ptr input, Eigen::VectorXf& paras, EdgeLine& output) {
	// for simplicity, only consider two dimension
	// need to improve

	PCL_WARN("@ptsToLine need to be improved, it is shorter than the real length ");
	float k1 = paras[4] / paras[3];
	//float b = paras[1] - k * paras[0];
	float b1 = input->points[0].y - k1 * input->points[0].x;


//	PointT min,max;
//	pcl::getMinMax3D(*input, min, max);

	float minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
	for (auto &p:input->points) {
		float k2 = -1/k1;
		float b2 = p.y - k2 * p.x;
		Eigen::Matrix2f A;
		Eigen::Vector2f b;
		A << k1,-1, k2, -1;
		b << -b1,-b2;
		Eigen::Vector2f x = A.colPivHouseholderQr().solve(b);
		minX = min (minX , x[0]);
		maxX = max (maxX , x[0]);

		minY = min (minY , x[1]);
		maxY = max (maxY , x[1]);
	}

	PointT p,q;
	p.z = 0; q.z = 0;
	if (k1>0) {
		p.x = minX;
		p.y = minY;
		q.x = maxX;
		q.y = maxY;
	}else {
		p.x = minX;
		p.y = maxY;
		q.x = maxX;
		q.y = minY;
	}


	Eigen::VectorXf coff(6);
	output.paras = coff;
	output.p = p;
	output.q = q;
}

void findLinkedLines(vector<EdgeLine>& edgeLines) {
	vector<PointT> allPts;
	for(auto& l: edgeLines) {
		allPts.push_back(l.p);
		allPts.push_back(l.q);
	}
	int n = allPts.size();
	std::unordered_map<int,int> map;

	for (int i = 0; i < n; i++) {
		float minDist = 100;
		int index = -1;
		for (int j = 0; j < n; j++) {
			if (i==j) continue;
			if (i%2 == 0 && j == i+1) continue;
			if (i%2 != 0 && j == i-1) continue;

			if (pcl::geometry::distance( allPts[i], allPts[j] ) < minDist) {
				minDist = pcl::geometry::distance( allPts[i], allPts[j] );
				index = j;
			}
		}

		// make sure dist between two points less than 1m
		if (minDist < 1) {
			assert(index >= 0);
			if (map.count(index) && map[index] == i) continue;
			EdgeLine line;
			line.p = allPts[i];
			line.q = allPts[index];
			edgeLines.push_back(line);
			map[i] = index;
		}

	}
}

void calculateMeanStandardDev(vector<float> v, float& mean, float& stdev) {
    float sum = std::accumulate(v.begin(), v.end(), 0.0);
    mean = sum / v.size();

    std::vector<float> diff(v.size());
    std::transform(v.begin(), v.end(), diff.begin(), [mean](double x) { return x - mean; });
    float sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    stdev = std::sqrt(sq_sum / v.size());
}

void regionGrow(PointCloudT::Ptr input, int NumberOfNeighbours, int SmoothnessThreshold, int CurvatureThreshold,
		int MinSizeOfCluster, int KSearch, vector<PointCloudT::Ptr>& outputClusters) {
	std::vector<pcl::PointIndices> clustersIndices;
	pcl::search::Search<PointT>::Ptr tree = boost::shared_ptr<pcl::search::Search<PointT> >(
			new pcl::search::KdTree<PointT>);
	pcl::PointCloud<pcl::Normal>::Ptr normals_all(new pcl::PointCloud<pcl::Normal>);
	calculateNormals(input, normals_all, KSearch);
	pcl::RegionGrowing<PointT, pcl::Normal> reg;
	reg.setMinClusterSize(0);
	reg.setMaxClusterSize(100000);
	reg.setSearchMethod(tree);
	reg.setNumberOfNeighbours(NumberOfNeighbours);
	reg.setInputCloud(input);
	reg.setInputNormals(normals_all);
	reg.setSmoothnessThreshold(static_cast<float>(SmoothnessThreshold / 180.0 * M_PI));
	reg.setCurvatureThreshold(CurvatureThreshold);
	reg.extract(clustersIndices);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr colored_cloud = reg.getColoredCloud();
	for (size_t i = 0; i < clustersIndices.size(); ++i) {
		if (clustersIndices[i].indices.size() < MinSizeOfCluster) continue;
		PointCloudT::Ptr singleCluster(new PointCloudT);
		for (auto &p : clustersIndices[i].indices) {
			singleCluster->points.push_back(input->points[p]);
		}
		outputClusters.push_back(singleCluster);
	}
}

void calculateNormals(PointCloudT::Ptr input, pcl::PointCloud <pcl::Normal>::Ptr &normals_all, int KSearch)
{
	//1-1. generating the normal for each point
    stringstream ss;
    cout << "The point you input doesn't contain normals, calculating normals...\n";
    pcl::search::Search<PointT>::Ptr tree = boost::shared_ptr<pcl::search::Search<PointT> >(new pcl::search::KdTree<PointT>);
    pcl::NormalEstimation<PointT, pcl::Normal> normal_estimator;
    normal_estimator.setSearchMethod(tree);
    normal_estimator.setInputCloud(input);
    normal_estimator.setKSearch(KSearch);
    normal_estimator.compute(*normals_all);

    for (size_t i = 0; i < normals_all->points.size(); ++i)
    {
        input->points[i].normal_x = normals_all->points[i].normal_x;
        input->points[i].normal_y = normals_all->points[i].normal_y;
        input->points[i].normal_z = normals_all->points[i].normal_z;
    }
}


void applyBeamExtraction(PointCloudT::Ptr input,float high, vector<EdgeLine>& edgeLines){
	float possibleHeight = 0.5;
	PointCloudT::Ptr inputTmp(new PointCloudT);
	pcl::copyPointCloud(*input,*inputTmp);
	pcl::PassThrough<PointT> filterZ;
	filterZ.setInputCloud(inputTmp);
	filterZ.setFilterFieldName("z");
	filterZ.setFilterLimits(high-possibleHeight, high - 0.1);
	filterZ.filter(*inputTmp);


	simpleView("[applyBeamExtraction] extract Z", inputTmp);

	PointCloudT::Ptr filterPts(new PointCloudT);
	VoxelGrid<PointT> sor;
	sor.setInputCloud(inputTmp);
	sor.setLeafSize(0.07,0.07,20);
	sor.filter(*filterPts);
	Eigen::Vector3i divisions =  sor.getNrDivisions();


	// put points index to each voxel
	vector<vector<int>> voxels(divisions[0]*divisions[1]*divisions[2], vector<int>());
	for (int i = 0; i < inputTmp->points.size(); ++i) {
		auto cord = sor.getGridCoordinates(inputTmp->points[i].x, inputTmp->points[i].y, inputTmp->points[i].z);
		cord = cord - sor.getMinBoxCoordinates();
		voxels[cord[0]*divisions[1]*divisions[2] + cord[1]*divisions[2] + cord[2]].push_back(i);
	}
	// test result
	int testSum = 0;
	int maxNum = 0;
	for (auto voxel:voxels) {
		testSum += voxel.size();
		maxNum = max(maxNum, (int)voxel.size());
	}

	assert(testSum == inputTmp->points.size());
	cout << "[applyBeamExtraction] maxNum " << maxNum << endl;
	// filter the voxel cell whose size is smaller than
	filterPts->resize(0);
	assert(filterPts->size() == 0);
	for (auto voxel:voxels) {
		if (voxel.size() > 10) for(auto &idx : voxel) filterPts->push_back(inputTmp->points[idx]);
	}

	cout << "[applyBeamExtraction]  before " << inputTmp->size() << "  after " << filterPts->size() << endl;

	simpleView("[applyBeamExtraction] filterP pts", filterPts);

	PointCloudT::Ptr removeLinesPts(new PointCloudT);
	removePtsAroundLine(filterPts,removeLinesPts, edgeLines, 0.5);
}

void removePtsAroundLine(PointCloudT::Ptr input, PointCloudT::Ptr output, vector<EdgeLine>& lines, float dist){
	PointCloudT::Ptr allPts(new PointCloudT);
	copyPointCloud(*input, *allPts);
	for (auto &p : allPts->points) p.z = 0;
	vector<PointCloudT::Ptr> linesPts;
	for (auto &line: lines) {
		PointCloudT::Ptr tmp(new PointCloudT);
		generateLinePointCloud(line.p, line.q, 100, 255 <<24 | 1255 << 16 | 0 << 8 | 0, tmp);
		linesPts.push_back(tmp);
		for (auto& p: tmp->points) allPts->push_back(p);
	}

	simpleView("[removePtsAroundLine] allPts" ,allPts);

	PointCloudT::Ptr filtered(new PointCloudT);
	VoxelGrid<PointT> sor;
	sor.setInputCloud(allPts);
	sor.setLeafSize(dist,dist,20);
	sor.filter(*filtered);
	Eigen::Vector3i divisions = sor.getNrDivisions();
	cout << divisions.transpose() << endl;
	unordered_set<int> filterGridIdx;
	for (auto &linePts : linesPts) {
		for (auto &p : linePts->points) {
			auto cord = sor.getGridCoordinates(p.x, p.y, p.z);
			cord = cord - sor.getMinBoxCoordinates();
			filterGridIdx.insert(cord[0]*divisions[1]*divisions[2] + cord[1]*divisions[2] + cord[2]);
		}
	}

	for (auto &p : input->points) {
		auto cord = sor.getGridCoordinates(p.x, p.y, p.z);
		cord = cord - sor.getMinBoxCoordinates();
		if (filterGridIdx.count(cord[0]*divisions[1]*divisions[2] + cord[1]*divisions[2] + cord[2])) continue;
		output->push_back(p);
	}
	cout << "[removePtsAroundLine] before " << input->size() << "  after " << output->size() << endl;
	simpleView("[removePtsAroundLine] ",output);
}

void regionGrow2D(PointCloudT::Ptr input, vector<PointCloudT::Ptr>& output){
    // calculate Normal
    PointCloud<Normal>::Ptr cloud_normals(new PointCloud<Normal>);
    calculateNormal2D(input, cloud_normals);

    float radius = Paras<float>("RegionGrow2D","serarchRadius");
    int normalTh = Paras<int>("RegionGrow2D","normalTh");
    pcl::KdTreeFLANN<PointT> kdtree;
    kdtree.setInputCloud(input);
    unordered_set<int> found;
    int k = 0;
    while(found.size() < input->size()) {
        vector<int> group;
        stack<PointT> seeds;

        while(found.count(k)) k++;
        if (k >= input->size()) break;
        seeds.push(input->points[k]);
        found.insert(k);
        group.push_back(k);
        Eigen::Vector4f seedNormal = cloud_normals->points[k].getNormalVector4fMap();

        while(!seeds.empty()) {
            PointT seed = seeds.top();
            seeds.pop();
            vector<int> pointIdx;
            vector<float> dist;
            kdtree.radiusSearch(seed, radius, pointIdx,dist);
            for(auto& ix : pointIdx) {
                if (found.count(ix)) continue;
                Eigen::Vector4f q = cloud_normals->points[ix].getNormalVector4fMap();
                auto r = seedNormal.dot(q);
                float angle_in_radiant = acos(r);
                if (angle_in_radiant < M_PI/180*normalTh) {
                    found.insert(ix);
                    seeds.push(input->points[ix]);
                    group.push_back(ix);
                }
            }
        }
        if (group.size() < 2) continue;
        PointCloudT::Ptr foundPts(new PointCloudT);
        for (auto& idx:group) {
            foundPts->points.push_back(input->points[idx]);
        }
        output.push_back(foundPts);
    }

    cout << "[regionGrow2D] " << output.size() << " clusters are found" << endl;
}

void calculateNormal2D(PointCloudT::Ptr input, PointCloud<Normal>::Ptr cloud_normals){
    KdTreeFLANN<PointT> kdtree;
    kdtree.setInputCloud(input);
    int K = 10;
    for (auto &p:input->points) {
        Eigen::Vector3f v; v << p.x, p.y, p.z ;
        std::vector<int> pointIdxNKNSearch(K);
        std::vector<float> pointNKNSquaredDistance(K);
        Eigen::MatrixXf conv = Eigen::MatrixXf::Zero(3,3);
        Normal normal; normal.normal_x = 0; normal.normal_y = 0; normal.normal_z = 0;
        if ( kdtree.nearestKSearch (p, K, pointIdxNKNSearch, pointNKNSquaredDistance) > 0 )
        {
            for (int & idx: pointIdxNKNSearch) {
                Eigen::Vector3f x_i;
                x_i << input->points[idx].x, input->points[idx].y, input->points[idx].z;
                conv = conv +  ( (v - x_i) * (v - x_i).transpose() );
            }
            Eigen::JacobiSVD<Eigen::MatrixXf> svd(conv,  Eigen::ComputeThinU | Eigen::ComputeThinV);
            normal.normal_x = svd.matrixU()(1,0);
            normal.normal_y = svd.matrixU()(1,1);
            normal.normal_z = svd.matrixU()(1,2);
            // cout << p.x << "," << p.y << "," << p.z << "," << svd.matrixU()(1,0) << ", " << svd.matrixU()(1,1) << ", " << svd.matrixU()(1,2) << ";\n";
        }
        cloud_normals->push_back(normal);
    }
}

void BeamRANSAC(PointCloudT::Ptr input, float high){
	float possibleHeight = 0.5;
	PointCloudT::Ptr inputTmp(new PointCloudT);
	pcl::copyPointCloud(*input,*inputTmp);
	pcl::PassThrough<PointT> filterZ;
	filterZ.setInputCloud(inputTmp);
	filterZ.setFilterFieldName("z");
	filterZ.setFilterLimits(high-possibleHeight, high - 0.1);
	filterZ.filter(*inputTmp);
	simpleView("[BeamRANSAC] extract Z", inputTmp);

	//regionGrow(inputTmp, int NumberOfNeighbours, int SmoothnessThreshold, int CurvatureThreshold,
			//int MinSizeOfCluster, int KSearch, vector<PointCloudT::Ptr>& outputClusters);
	vector<PointCloudT::Ptr> outputClusters;
	regionGrow(inputTmp, 30, 2, 2, 30, 10, outputClusters);

	vector<PointCloudT::Ptr> possibleBeamPlanes;
	vector<Eigen::Vector4f> possibleBeamPlanesCoff;
	for (auto& cluster: outputClusters) {
		pcl::ModelCoefficients::Ptr sacCoefficients(new pcl::ModelCoefficients);
		pcl::PointIndices::Ptr sacInliers(new pcl::PointIndices);
		pcl::SACSegmentation<PointT> seg;
		seg.setOptimizeCoefficients(true);
		seg.setModelType(pcl::SACMODEL_PLANE);
		seg.setMethodType(pcl::SAC_RANSAC);
		seg.setDistanceThreshold(0.05);
		seg.setInputCloud(cluster);
		seg.segment(*sacInliers, *sacCoefficients);
		Eigen::Vector4f  coff;
		coff << sacCoefficients->values[0], sacCoefficients->values[1], sacCoefficients->values[2], sacCoefficients->values[3];
		// we only need to remain the planes which are very parallel with xy plane
		if (abs(coff[0]) < 0.1 && abs(coff[1]) < 0.1){
			possibleBeamPlanes.push_back(cluster);
			possibleBeamPlanesCoff.push_back(coff);
		}
	}
	cout << "[BeamRANSAC] " << possibleBeamPlanes.size() << " are found from " << outputClusters.size() << endl;

	// next step, we want to construct lines around found points and check these lines up space whether contain pts
	simpleView("[BeamRANSAC] possible Beam Planes", possibleBeamPlanes);
	vector<PointCloudT::Ptr> tmp;
	for (auto& plane: possibleBeamPlanes) {
		PointCloudT::Ptr resTmp(new PointCloudT);
		vector<PointT> edge = findEdgeForPlane(plane, high, inputTmp);
		if (edge.size() == 0) continue;
		generateLinePointCloud(edge[0],edge[1],100, INT32_MAX, resTmp);
		generateLinePointCloud(edge[1],edge[2],100, INT32_MAX, resTmp);
		generateLinePointCloud(edge[2],edge[3],100, INT32_MAX, resTmp);
		generateLinePointCloud(edge[3],edge[0],100, INT32_MAX, resTmp);
		tmp.push_back(resTmp);
	}
	for (auto& t : tmp) possibleBeamPlanes.push_back(t);

	simpleView("[BeamRANSAC] possible Beam Planes with check boundary", possibleBeamPlanes);


}

vector<PointT> findEdgeForPlane(PointCloudT::Ptr input, float maxRoomZ, PointCloudT::Ptr roofPart){
    // 1 - determine the direction of the plane
	pcl::ModelCoefficients::Ptr sacCoefficients(new pcl::ModelCoefficients);
	pcl::PointIndices::Ptr sacInliers(new pcl::PointIndices);
	pcl::SACSegmentation<PointT> seg;
	seg.setOptimizeCoefficients(true);
	seg.setModelType(pcl::SACMODEL_LINE);
	seg.setMethodType(pcl::SAC_RANSAC);
	seg.setDistanceThreshold(Paras<float>("BeamRecons","RANSACDistTh"));
	seg.setInputCloud(input);
	seg.segment(*sacInliers, *sacCoefficients);
	Eigen::VectorXf coff(6);
	coff << sacCoefficients->values[0], sacCoefficients->values[1], sacCoefficients->values[2],
			sacCoefficients->values[3], sacCoefficients->values[4], sacCoefficients->values[5];
	//cout << coff[3]<< " " << coff[4] << " " << coff[5]<< " " <<endl;

	// 2 - rotate the plane according to the angle found
	PointCloudT::Ptr transformed(new PointCloudT);
	Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
	float theta = acos(abs(coff[4]) / sqrt(coff[3]*coff[3] + coff[4]*coff[4] + coff[5]*coff[5]));
	if (coff[3]*coff[4] < 0) theta = -theta;
	transform(0,0) = cos(theta); transform(0,1) = -sin(theta); transform(1,0) = sin(theta); transform(1,1) = cos(theta);
	transformPointCloud(*input, *transformed, transform);

    PointCloudT::Ptr roofPartTrans(new PointCloudT);
    transformPointCloud(*roofPart, *roofPartTrans, transform);
	//simpleView("[findEdgeForPlane] transformed pts", transformed);

    // 3 - define edges : transform so it can be parallel with x or y axis
	PointT minP, maxP, p1, p2, p3, p4;
	getMinMax3D(*transformed, minP, maxP);
	p1.x = minP.x; p1.y = minP.y; p1.z = maxP.z;
	p2.x = minP.x; p2.y = maxP.y; p2.z = maxP.z;
	p3.x = maxP.x; p3.y = maxP.y; p3.z = maxP.z;
	p4.x = maxP.x; p4.y = minP.y; p4.z = maxP.z;

	// 4 - determine the points in the rectangular shape (to verify whether it is a beam)
    ConditionAnd<PointT>::Ptr range_cond (new ConditionAnd<PointT> ());
    range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("x", ComparisonOps::GT, minP.x)));
    range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("x", ComparisonOps::LT, maxP.x)));
    range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("y", ComparisonOps::GT, minP.y)));
    range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("y", ComparisonOps::LT, maxP.y)));
    range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("z", ComparisonOps::GT, maxP.z+0.05)));
    range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("z", ComparisonOps::LT, maxRoomZ-0.05)));
    //simpleView("roofPartTrans", roofPartTrans);

    ConditionalRemoval<PointT> condrem;
    condrem.setCondition (range_cond);
    condrem.setInputCloud (roofPartTrans);
	PointCloudT::Ptr filtered(new PointCloudT);
    condrem.filter (*filtered);
    // todo: we can calculate how many points insides the cube. if it is more than a certain num, then it is not beam
    //cout << "num of points left " << filtered->size() << " from" <<  roofPart->size() << endl;
	//simpleView("trans -  filtered",filtered);

	// 6 - check lines in Z direction whether has num of points
	PointCloudT::Ptr debugCube(new PointCloudT);
	copyPointCloud(*roofPartTrans,*debugCube);
	int numPtsZ1 = checkNumofPointofLineinZ(p1,p2, roofPartTrans, maxP.z, maxRoomZ,debugCube);
	int numPtsZ2 = checkNumofPointofLineinZ(p2,p3, roofPartTrans, maxP.z, maxRoomZ,debugCube);
	int numPtsZ3 = checkNumofPointofLineinZ(p3,p4, roofPartTrans, maxP.z, maxRoomZ,debugCube);
	int numPtsZ4 = checkNumofPointofLineinZ(p4,p1, roofPartTrans, maxP.z, maxRoomZ,debugCube);
	if (Paras<bool>("View","BeamCube")) simpleView("[findEdgeForPlane] BeamCube", debugCube);
	// test Part
/*	PointT testA,testB,testC,testD;
	testA.x = minP.x; testA.y = minP.y; testA.z = maxP.z;
	testB.x = minP.x; testB.y = maxP.y; testB.z = maxP.z;
	testC.x = maxP.x; testC.y = maxP.y; testC.z = maxP.z;
	testD.x = maxP.x; testD.y = minP.y; testD.z = maxP.z;
	generateLinePointCloud(testA ,testB, 100, INT32_MAX, roofPartTrans);
	generateLinePointCloud(testB ,testC, 100, INT32_MAX, roofPartTrans);
	generateLinePointCloud(testC ,testD, 100, INT32_MAX, roofPartTrans);
	generateLinePointCloud(testD ,testA, 100, INT32_MAX, roofPartTrans);
	simpleView("trans -  with 4 edges",roofPartTrans);*/


	// 7 - check whether it is valid beam by check ratio of num. pts over cube size
	float distTh = Paras<float>("BeamRecons","EdgeZDist");
	assert( abs(p1.z-p2.z) < 0.1 && abs(p2.z-p3.z) < 0.1 && abs(p3.z-p4.z) < 0.1 && abs(p4.z-p1.z) < 0.1);
	float cubeSize1 = (geometry::distance(p1,p2)) * (maxRoomZ - p1.z) *  distTh * 2 * 1000;
	float cubeSize2 = (geometry::distance(p2,p3)) * (maxRoomZ - p2.z) *  distTh * 2 * 1000;
	float cubeSize3 = (geometry::distance(p3,p4)) * (maxRoomZ - p3.z) *  distTh * 2 * 1000;
	float cubeSize4 = (geometry::distance(p4,p1)) * (maxRoomZ - p4.z) *  distTh * 2 * 1000;
	assert( cubeSize1 > 0 && cubeSize2 > 0 && cubeSize3 > 0 && cubeSize4 > 0);
    cout << numPtsZ1/cubeSize1 << " "<< numPtsZ2/cubeSize2 << " "<< numPtsZ3/cubeSize3 << " "<< numPtsZ4/cubeSize4 << endl;
	// just make sure this way works in different environment
	assert( ((numPtsZ1 > -1) + (numPtsZ2 > -1) + (numPtsZ3 > -1) + (numPtsZ4 > -1)) == 4 );
	if ( ( (numPtsZ1/cubeSize1 >= Paras<float>("BeamRecons","cubeNumPtsRatio")) +
		   (numPtsZ2/cubeSize2 >= Paras<float>("BeamRecons","cubeNumPtsRatio")) +
		   (numPtsZ3/cubeSize3 >= Paras<float>("BeamRecons","cubeNumPtsRatio")) +
		   (numPtsZ4/cubeSize4 >= Paras<float>("BeamRecons","cubeNumPtsRatio"))) < 2 ) return vector<PointT>{};
	// 5 - reverse
	Eigen::Affine3f reverseT = Eigen::Affine3f::Identity();
	reverseT.rotate(Eigen::AngleAxisf(-theta,Eigen::Vector3f::UnitZ()));
	p1 = transformPoint(p1,reverseT);
	p2 = transformPoint(p2,reverseT);
	p3 = transformPoint(p3,reverseT);
	p4 = transformPoint(p4,reverseT);

/*	generateLinePointCloud(p1,p2,100, INT32_MAX, transformed);
	generateLinePointCloud(p2,p3,100, INT32_MAX, transformed);
	generateLinePointCloud(p3,p4,100, INT32_MAX, transformed);
	generateLinePointCloud(p4,p1,100, INT32_MAX, transformed);*/

	return vector<PointT>{p1,p2,p3,p4};

}

int checkNumofPointofLineinZ(PointT p, PointT q, PointCloudT::Ptr input, float minZ, float maxZ, PointCloudT::Ptr debug_cube){

	float distTh = Paras<float>("BeamRecons","EdgeZDist");
	float k = (p.y - q.y) / (p.x - q.x);
	float theta = -atan(k);

	PointCloudT::Ptr transformed(new PointCloudT);
	Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
	transform(0,0) = cos(theta); transform(0,1) = -sin(theta); transform(1,0) = sin(theta); transform(1,1) = cos(theta);
	transformPointCloud(*input, *transformed, transform);

	PointT r_p, r_q;
	Eigen::Affine3f T = Eigen::Affine3f::Identity();
	T.rotate(Eigen::AngleAxisf(theta,Eigen::Vector3f::UnitZ()));
	r_p = transformPoint(p,T); r_q = transformPoint(q,T);

	//cout << p.y << " " << q.y << " " << p.x << " " << q.x << "  " << k << " theta:" << theta*180/M_PI <<  endl;
	if (abs(r_p.y-r_q.y) > 0.1) { cout << "r_p.y:" << r_p.y << "  r_q.y:" << r_q.y  << endl; abort();}
	ConditionAnd<PointT>::Ptr range_cond (new ConditionAnd<PointT> ());

	range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("x", ComparisonOps::GT, min(r_p.x,r_q.x))));
	range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("x", ComparisonOps::LT, max(r_p.x,r_q.x))));
	range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("y", ComparisonOps::GT, r_p.y-distTh)));
	range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("y", ComparisonOps::LT, r_p.y+distTh)));
	range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("z", ComparisonOps::GT, minZ)));
	range_cond->addComparison (FieldComparison<PointT>::ConstPtr (new FieldComparison<PointT> ("z", ComparisonOps::LT, maxZ)));
	ConditionalRemoval<PointT> condrem;
	condrem.setCondition (range_cond);
	condrem.setInputCloud (transformed);
	PointCloudT::Ptr filtered(new PointCloudT);
	condrem.filter (*filtered);

	// debug part
	if (Paras<bool>("View","BeamCube")) {
		PointT a = r_p, b = r_q, c = r_q, d = r_p, a2 = r_p, b2 = r_q, c2 = r_q, d2 = r_p;
		a.y -= 0.1;  b.y -= 0.1; c.y += 0.1;  d.y += 0.1;
		a.z = minZ; b.z =minZ; c.z = minZ; d.z = minZ;
		a2.y -= 0.1;  b2.y -= 0.1; c2.y += 0.1;  d2.y += 0.1;
		a2.z = maxZ; b2.z =maxZ; c2.z = maxZ; d2.z = maxZ;
		Eigen::Affine3f reverseT = Eigen::Affine3f::Identity();
		reverseT.rotate(Eigen::AngleAxisf(-theta,Eigen::Vector3f::UnitZ()));
		r_p = transformPoint(r_p,reverseT); r_q = transformPoint(r_q,reverseT);
		a = transformPoint(a,reverseT); b = transformPoint(b,reverseT);c = transformPoint(c,reverseT); d = transformPoint(d,reverseT);
		a2 = transformPoint(a2,reverseT); b2 = transformPoint(b2,reverseT);c2 = transformPoint(c2,reverseT); d2 = transformPoint(d2,reverseT);
		generateLinePointCloud(r_p,r_q,100,Colors.White, debug_cube);
		generateLinePointCloud(a,b,c,d,100,Colors.Red, debug_cube);
		generateLinePointCloud(a2,b2,c2,d2,100,Colors.Blue, debug_cube);
		if (Paras<bool>("View","BeamCubeEach")) simpleView("[checkNumofPointofLineinZ] test check num of pts inside cube", debug_cube);
	}

	return filtered->size();

}

void exportToDxf(string outputPath, vector<EdgeLine>& lines, float minZ, float maxZ){
    KKRecons::DxfExporter exporter;

    for (auto& line: lines) {
        PointXYZ a,b,c,d;
        a.x = line.p.x; a.y = line.p.y; a.z = minZ;
        b.x = line.q.x; b.y = line.q.y; b.z = minZ;
        c.x = line.q.x; c.y = line.q.y; c.z = maxZ;
        d.x = line.p.x; d.y = line.p.y; d.z = maxZ;
        DxfFace face(a,b,c,d);
        exporter.insert(face);
        break;
    }
    exporter.exportDXF(outputPath);
}