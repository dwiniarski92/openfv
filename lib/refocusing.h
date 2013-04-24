// -------------------------------------------------------
// -------------------------------------------------------
// Synthetic Aperture - Particle Tracking Velocimetry Code
// --- Refocusing Library Header ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#ifndef REFOCUSING_LIBRARY
#define REFOCUSING_LIBRARY

#include "std_include.h"
#include "calibration.h"
#include "typedefs.h"

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace std;
using namespace cv;

class saRefocus {

 public:
    ~saRefocus() {

    }

 saRefocus(refocusing_data refocusing_params):
    P_mats_(refocusing_params.P_mats), P_mats_u_(refocusing_params.P_mats_u), cam_names_(refocusing_params.cam_names), img_size_(refocusing_params.img_size), scale_(refocusing_params.scale), num_cams_(refocusing_params.num_cams) {}

    // Functions

    void startGPUsession();
    void initializeGPU();
    void GPUrefocus(double z);

 private:

    // data types and private functions
    vector<Mat> P_mats_;
    vector<Mat> P_mats_u_;
    vector<string> cam_names_;
    Size img_size_;
    double scale_;
    int num_cams_;

    double z;

    vector<Mat> array_host;
    Mat refocused_host;
    Mat den_host;

    vector<gpu::GpuMat> array;
    gpu::GpuMat temp;
    gpu::GpuMat temp2;
    gpu::GpuMat refocused;
    

};

#endif
