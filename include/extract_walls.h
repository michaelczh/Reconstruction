//
// Created by czh on 11/4/18.
//

#ifndef TEST_PCL_EXTRACTWALL_H
#define TEST_PCL_EXTRACTWALL_H

#include <iostream>
#include <vector>
#include <pcl/point_types.h>
#include <pcl/search/search.h>
#include <pcl/search/kdtree.h>
#include <pcl/features/normal_3d.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/segmentation/sac_segmentation.h> //RANSAC‚Ì‚½‚ß
#include <math.h>
#include <pcl/filters/project_inliers.h> //•½–Ê‚É“Š‰e‚·‚é‚½‚ß
#include <pcl/filters/passthrough.h>
#include <pcl/common/pca.h>
#include <pcl/common/common.h>
#include <pcl/filters/voxel_grid.h> //ƒ_ƒEƒ“ƒTƒ“ƒvƒŠƒ“ƒO‚Ì‚½‚ß
#include <pcl/common/geometry.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/conditional_removal.h>
#include "Plane.h"
using namespace std;
using namespace pcl;

struct reconstructParas
{
    // Downsampling
    int KSearch = 10;
    float leafSize = 0.05; // unit is meter -> 5cm

    // Clustering
    int MinSizeOfCluster = 800;
    int NumberOfNeighbours = 30;
    int SmoothnessThreshold = 5; // angle 360 degree
    int CurvatureThreshold = 10;
    // RANSAC
    double RANSAC_DistThreshold = 0.25; //0.25;
    float RANSAC_MinInliers = 0.5; // 500 todo: should be changed to percents
    float RANSAC_PlaneVectorThreshold = 0.2;

    // Fill the plane
    int pointPitch = 20; // number of point in 1 meter

    // Combine planes
    int minimumEdgeDist = 1; //we control the distance between two edges and the height difference between two edges
    float minHeightDiff = 0.5;
    int minAngle_normalDiff = 5;// when extend smaller plane to bigger plane, we will calculate the angle between normals of planes

    int roof_NumberOfNeighbours = 1;
    int roof_SmoothnessThreshold = 2;
    int roof_CurvatureThreshold = 1;
    int roof_MinSizeOfCluster = 1;
};

struct EdgeLine{
    Eigen::VectorXf paras;
    PointT p;
    PointT q;
};

struct Colors{
    const int32_t Red    = 255 <<24 | 255 << 16 | 0   << 8 | 0;
    const int32_t Yellow = 255 <<24 | 255 << 16 | 255 << 8 | 0;
    const int32_t Blue   = 255 <<24 | 0   << 16 | 0   << 8 | 255;
    const int32_t Green  = 255 <<24 | 0   << 16 | 255 << 8 | 0;
    const int32_t Cyan   = 255 <<24 | 0   << 16 | 255 << 8 | 255;
    const int32_t Pink   = 255 <<24 | 255 << 16 | 0   << 8 | 255;
    const int32_t White  = INT32_MAX;
}Colors;
typedef pcl::PointXYZRGBNormal PointT;
typedef pcl::PointCloud<PointT> PointCloudT;

void generateLinePointCloud(PointT pt1, PointT pt2, int pointPitch, int color, PointCloudT::Ptr output);
void generateLinePointCloud(PointT a, PointT b, PointT c, PointT d, int pointPitch, int color, PointCloudT::Ptr output);

// mark: for debug reason
void extendSmallPlaneToBigPlane(Plane& sourceP, Plane& targetP, int color, int pointPitch, PointCloudT::Ptr output);
bool onSegment(PointT p, PointT q, PointT r);
float orientation(PointT p, PointT q, PointT r);
bool isIntersect(PointT p1, PointT q1, PointT p2, PointT q2);
void smoothNoise(PointCloudT::Ptr input, int K, float beta);
void densityFilter(PointCloudT::Ptr input);

void detectHeightRange(PointCloudT::Ptr input, float& high, float& low);

void extractTopPts(PointCloudT::Ptr input, PointCloudT::Ptr output, float highest, float dimension, reconstructParas paras);
void convert2D(PointCloudT::Ptr input,PointCloudT::Ptr output);
void findBiggestComponent2D(PointCloudT::Ptr input, PointCloudT::Ptr output);

void computeHull(PointCloudT::Ptr input, PointCloudT::Ptr output, float alpha);
void extractLineFromEdge(PointCloudT::Ptr input, vector<EdgeLine>& edgeLines);
void extractLineFromEdge2(PointCloudT::Ptr input, vector<EdgeLine>& edgeLines);
void seperatePtsToGroups(PointCloudT::Ptr input, float radius, vector<PointCloudT::Ptr>& output);
void ptsToLine(PointCloudT::Ptr input, Eigen::VectorXf& paras, EdgeLine& output);
void findLinkedLines(vector<EdgeLine>& edgeLines);

void calculateMeanStandardDev(vector<float> v, float& mean, float& stdev);

void calculateNormals(PointCloudT::Ptr input, pcl::PointCloud <pcl::Normal>::Ptr &normals_all, int KSearch);
void regionGrow(PointCloudT::Ptr input, int NumberOfNeighbours, int SmoothnessThreshold, int CurvatureThreshold,
                int MinSizeOfCluster, int KSearch, vector<PointCloudT::Ptr>& outputClusters);

void applyBeamExtraction(PointCloudT::Ptr input, float high, vector<EdgeLine>& edgeLines);

void removePtsAroundLine(PointCloudT::Ptr input, PointCloudT::Ptr output, vector<EdgeLine>& lines, float dist);

void regionGrow2D(PointCloudT::Ptr input, vector<PointCloudT::Ptr>& output);
void calculateNormal2D(PointCloudT::Ptr input, PointCloud<Normal>::Ptr cloud_normals);

void BeamRANSAC(PointCloudT::Ptr input, float high);
vector<PointT> findEdgeForPlane(PointCloudT::Ptr input, float maxRoomZ, PointCloudT::Ptr roofPart);

void exportToDxf(string outputPath, vector<EdgeLine>& lines, float minZ, float maxZ);

int checkNumofPointofLineinZ(PointT p, PointT q, PointCloudT::Ptr input, float minZ, float maxZ, PointCloudT::Ptr debug_cube);
#endif //TEST_PCL_EXTRACTWALL_H
