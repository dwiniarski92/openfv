// -------------------------------------------------------
// -------------------------------------------------------
// Synthetic Aperture - Particle Tracking Velocimetry Code
// --- Refocusing Library ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#include "std_include.h"
#include "calibration.h"
#include "refocusing.h"
#include "tools.h"
#include "cuda_lib.h"

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace std;
using namespace cv;

void saRefocus::read_calib_data(string path) {
 
    ifstream file;

    file.open(path.c_str());
    cout<<"Loading calibration data...";

    file>>num_cams_;

    for (int n=0; n<num_cams_; n++) {
        
        Mat_<double> P_mat = Mat_<double>::zeros(3,4);
        for (int i=0; i<3; i++) {
            for (int j=0; j<4; j++) {
                file>>P_mat(i,j);
            }
        }
        P_mats_.push_back(P_mat);

        Mat_<double> loc = Mat_<double>::zeros(3,1);
        for (int i=0; i<3; i++)
            file>>loc(i,0);

        cam_locations_.push_back(loc);

    }
   
    file>>zW_;
    file>>t_;
    file>>n1_;
    file>>n2_;
    file>>n3_;

    file>>img_size_.width;
    file>>img_size_.height;
    file>>scale_;

    cout<<"done!"<<endl;

}

void saRefocus::read_imgs(string path) {

    DIR *dir;
    struct dirent *ent;
 
    string dir1(".");
    string dir2("..");
    string temp_name;
    string img_prefix = "";

    Mat image, fimage;

    vector<string> img_names;

    cout<<"\nREADING IMAGES TO REFOCUS...\n\n";

    for (int i=0; i<num_cams_; i++) {

        cout<<"Camera "<<i+1<<" of "<<num_cams_<<"..."<<endl;

        string path_tmp;
        vector<Mat> refocusing_imgs_sub;

        path_tmp = path+cam_names_[i]+"/"+img_prefix;
        
        dir = opendir(path_tmp.c_str());
        while(ent = readdir(dir)) {
            temp_name = ent->d_name;
            if (temp_name.compare(dir1)) {
                if (temp_name.compare(dir2)) {
                    string path_img = path_tmp+temp_name;
                    img_names.push_back(path_img);
                }
            }
        }

        sort(img_names.begin(), img_names.end());
        for (int i=0; i<img_names.size(); i++) {
            //cout<<i<<": "<<img_names[i]<<endl;
            image = imread(img_names[i], 0);
            image.convertTo(fimage, CV_32F);
            //image.convertTo(fimage, CV_8U);
            refocusing_imgs_sub.push_back(fimage.clone());
        }
        img_names.clear();

        imgs.push_back(refocusing_imgs_sub);
        path_tmp = "";

        cout<<"done!\n";
   
    }
 
    cout<<"\nDONE READING IMAGES!\n\n";

}

void saRefocus::read_imgs_mtiff(string path) {
    
    DIR *dir;
    struct dirent *ent;

    string dir1(".");
    string dir2("..");
    string temp_name;

    vector<string> img_names;

    dir = opendir(path.c_str());
    while(ent = readdir(dir)) {
        temp_name = ent->d_name;
        if (temp_name.compare(dir1)) {
            if (temp_name.compare(dir2)) {
                if (temp_name.compare(temp_name.size()-3,3,"tif") == 0) {
                    string img_name = path+temp_name;
                    img_names.push_back(img_name);
                }
            }
        }
    }

    vector<TIFF*> tiffs;
    for (int i=0; i<img_names.size(); i++) {
        TIFF* tiff = TIFFOpen(img_names[i].c_str(), "r");
        tiffs.push_back(tiff);
    }

    cout<<"Counting number of frames...";
    int dircount = 0;
    if (tiffs[0]) {
	do {
	    dircount++;
	} while (TIFFReadDirectory(tiffs[0]));
    }
    cout<<"done! "<<dircount<<" frames found."<<endl<<endl;

    cout<<"Reading images..."<<endl;
    for (int i=0; i<img_names.size(); i++) {
        
        cout<<"Camera "<<i+1<<"...";

        vector<Mat> refocusing_imgs_sub;

        int frame=0;
        int count=0;
        int skip=1400;
        while (frame<dircount) {

            Mat img;
            uint32 c, r;
            size_t npixels;
            uint32* raster;
            
            TIFFSetDirectory(tiffs[i], frame);

            TIFFGetField(tiffs[i], TIFFTAG_IMAGEWIDTH, &c);
            TIFFGetField(tiffs[i], TIFFTAG_IMAGELENGTH, &r);
            npixels = r * c;
            raster = (uint32*) _TIFFmalloc(npixels * sizeof (uint32));
            if (raster != NULL) {
                if (TIFFReadRGBAImageOriented(tiffs[i], c, r, raster, ORIENTATION_TOPLEFT, 0)) {
                    img.create(r, c, CV_32F);
                    for (int i=0; i<r; i++) {
                        for (int j=0; j<c; j++) {
                            img.at<float>(i,j) = TIFFGetR(raster[i*c+j])/255.0;
                        }
                    }
                }
                _TIFFfree(raster);
            }
            refocusing_imgs_sub.push_back(img);
            count++;
            
            frame += skip;

        }

        imgs.push_back(refocusing_imgs_sub);
        cout<<"done! "<<count<<" frames read."<<endl;

    }

}

void saRefocus::GPUliveView() {

    initializeGPU();

    active_frame_ = 0;

    namedWindow("Result", CV_WINDOW_AUTOSIZE);       
    GPUrefocus(z, thresh, 1, active_frame_);
    
    double dz = 0.5;
    double dthresh = 5;

    while( 1 ){
        int key = cvWaitKey(10);
        //cout<<(key & 255)<<endl;
        if( (key & 255)==83 ) {
            z += dz;
            GPUrefocus(z, thresh, 1, active_frame_);
        } else if( (key & 255)==81 ) {
            z -= dz;
            GPUrefocus(z, thresh, 1, active_frame_);
        } else if( (key & 255)==82 ) {
            if (thresh<255) { 
                thresh += dthresh; 
                GPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==84 ) {
            if (thresh>0) { 
                thresh -= dthresh; 
                GPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==46 ) {
            if (active_frame_<array_all.size()) { 
                active_frame_++; 
                GPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==44 ) {
            if (active_frame_<array_all.size()) { 
                active_frame_--; 
                GPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==27 ) {
            break;
        }
    }

}

void saRefocus::CPUliveView() {

    active_frame_ = 0;

    namedWindow("Result", CV_WINDOW_AUTOSIZE);       
    CPUrefocus(z, thresh, 1, active_frame_);
    
    double dz = 0.5;
    double dthresh = 5;

    while( 1 ){
        int key = cvWaitKey(10);
        //cout<<(key & 255)<<endl;
        if( (key & 255)==83 ) {
            z += dz;
            CPUrefocus(z, thresh, 1, active_frame_);
        } else if( (key & 255)==81 ) {
            z -= dz;
            CPUrefocus(z, thresh, 1, active_frame_);
        } else if( (key & 255)==82 ) {
            if (thresh<255) { 
                thresh += dthresh; 
                CPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==84 ) {
            if (thresh>0) { 
                thresh -= dthresh; 
                CPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==46 ) {
            if (active_frame_<array_all.size()) { 
                active_frame_++; 
                CPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==44 ) {
            if (active_frame_<array_all.size()) { 
                active_frame_--; 
                CPUrefocus(z, thresh, 1, active_frame_); 
            }
        } else if( (key & 255)==27 ) {
            break;
        }
    }

}

// TODO: This function prints free memory on GPU and then
//       calls uploadToGPU() which uploads either a given
//       frame or all frames to GPU depending on frame_
void saRefocus::initializeGPU() {

    cout<<endl<<"INITIALIZING GPU FOR VISUALIZATION..."<<endl;
    cout<<"CUDA Enabled GPU Devices: "<<gpu::getCudaEnabledDeviceCount<<endl;
    
    gpu::DeviceInfo gpuDevice(gpu::getDevice());
    
    //cout<<"---"<<gpuDevice.name()<<"---"<<endl;
    //cout<<"Total Memory: "<<(gpuDevice.totalMemory()/pow(1024.0,2))<<" MB"<<endl;
    //cout<<"Free Memory: "<<(gpuDevice.freeMemory()/pow(1024.0,2))<<" MB"<<endl;

    uploadToGPU();

}

// TODO: Right now this function just starts uploading images
//       without checking if there is enough free memory on GPU
//       or not.
void saRefocus::uploadToGPU() {

    //gpu::DeviceInfo gpuDevice(gpu::getDevice());
    //double free_mem_GPU = gpuDevice.freeMemory()/pow(1024.0,2);
    //cout<<"Free Memory before: "<<free_mem_GPU<<" MB"<<endl;

    double factor = 0.9;

    if (frame_>=0) {

        cout<<"Uploading frame "<<frame_<<" to GPU..."<<endl;
        for (int i=0; i<num_cams_; i++) {
            temp.upload(imgs[i][frame_]);
            array.push_back(temp.clone());
        }
        array_all.push_back(array);

    } else if (frame_==-1) {
        
        cout<<"Uploading all frame to GPU..."<<endl;
        for (int i=0; i<imgs[0].size(); i++) {
            for (int j=0; j<num_cams_; j++) {
                temp.upload(imgs[j][i]);
                //gpu::Canny(temp, temp2, 100, 200);
                array.push_back(temp.clone());
            }
            array_all.push_back(array);
            array.clear();
        }
        
    } else {
        cout<<"Invalid frame value to visualize!"<<endl;
    }

    //cout<<"Free Memory after: "<<(gpuDevice.freeMemory()/pow(1024.0,2))<<" MB"<<endl;

}

void saRefocus::GPUrefocus(double z, double thresh, int live, int frame) {

    z *= warp_factor_;

    Scalar fact = Scalar(1/double(array_all[frame].size()));

    Mat H, trans;
    T_from_P(P_mats_u_[0], H, z, scale_, img_size_);
    gpu::warpPerspective(array_all[frame][0], temp, H, img_size_);

    if (mult_) {
        gpu::pow(temp, mult_exp_, temp2);
    } else {
        gpu::multiply(temp, fact, temp2);
    }

    refocused = temp2.clone();

    for (int i=1; i<num_cams_; i++) {
        
        T_from_P(P_mats_u_[i], H, z, scale_, img_size_);
        
        gpu::warpPerspective(array_all[frame][i], temp, H, img_size_);

        if (mult_) {
            gpu::pow(temp, mult_exp_, temp2);
            gpu::multiply(refocused, temp2, refocused);
        } else {
            gpu::multiply(temp, fact, temp2);
            gpu::add(refocused, temp2, refocused);        
        }

    }
    
    gpu::threshold(refocused, refocused, thresh, 0, THRESH_TOZERO);

    Mat refocused_host_(refocused);
    //refocused_host_ /= 255.0;

    if (live) {
        //refocused_host_ /= 255.0;
        char title[50];
        sprintf(title, "z = %f, thresh = %f, frame = %d", z/warp_factor_, thresh, frame);
        putText(refocused_host_, title, Point(10,20), FONT_HERSHEY_PLAIN, 1.0, Scalar(255,0,0));
        //line(refocused_host_, Point(646,482-5), Point(646,482+5), Scalar(255,0,0));
        //line(refocused_host_, Point(646-5,482), Point(646+5,482), Scalar(255,0,0));
        imshow("Result", refocused_host_);
    }

    refocused_host_.convertTo(result, CV_8U);

}

void saRefocus::GPUrefocus_ref() {

    // -----------------------------------------------------//
    cout<<"Uploading required data to GPU...";
    Mat ptemp, pltemp;
    for (int i=0; i<num_cams_; i++) {

        P_mats_[i].convertTo(ptemp, CV_32FC1);
        pmat.upload(ptemp);
        P_mats_gpu.push_back(pmat.clone());

        cam_locations_[i].convertTo(pltemp, CV_32FC1);
        ploc.upload(pltemp);
        cam_locations_gpu.push_back(ploc.clone());

    }

    Mat_<float> D = Mat_<float>::zeros(3,3);
    D(0,0) = scale_; D(1,1) = scale_;
    D(0,2) = img_size_.width*0.5; D(1,2) = img_size_.height*0.5;
    D(2,2) = 1;

    Mat_<float> geom = Mat_<double>(1,5);
    geom(0,0) = zW_; geom(0,1) = n1_; geom(0,2) = n2_; geom(0,3) = n3_; geom(0,4) = t_;
    geom_gpu.upload(geom);

    PixToPhys.upload(D.inv());

    xmap.create(img_size_.height, img_size_.width, CV_32FC1);
    ymap.create(img_size_.height, img_size_.width, CV_32FC1);
    cout<<"done!"<<endl;
    // -----------------------------------------------------//

    // Refocusing an image:
    double z = 0.1;
    Scalar fact = Scalar(1/double(num_cams_));

    temp.create(img_size_.height, img_size_.width, CV_32FC1);
    temp2.create(img_size_.height, img_size_.width, CV_32FC1);
    refocused.create(img_size_.height, img_size_.width, CV_32FC1);
    
    for (int i=0; i<num_cams_; i++) {

        gpu_calc_refocus_map(xmap, ymap, PixToPhys, P_mats_gpu[i], cam_locations_gpu[i], geom_gpu, z);
        gpu::remap(array_all[0][i], temp, xmap, ymap, INTER_LINEAR);
        gpu::multiply(temp, fact, temp2);
        gpu::add(refocused, temp2, refocused);

    }
    
    
    refocused.download(refocused_host_);
    imshow("result", refocused_host_); waitKey(0);

}

void saRefocus::CPUrefocus_ref() {

    double z = 0.1;

    Mat_<double> x = Mat_<double>::zeros(img_size_.height, img_size_.width);
    Mat_<double> y = Mat_<double>::zeros(img_size_.height, img_size_.width);
    cout<<"Calculating map for cam "<<0<<endl;
    calc_ref_refocus_map(cam_locations_[0], z, x, y, 0);
    Mat res, xmap, ymap;
    x.convertTo(xmap, CV_32FC1);
    y.convertTo(ymap, CV_32FC1);
    remap(imgs[0][0], res, xmap, ymap, INTER_LINEAR);

    Mat refocused = res.clone()/9.0;

    double wall_timer = omp_get_wtime();

    for (int i=1; i<num_cams_; i++) {

        cout<<"Calculating map for cam "<<i<<endl;
        calc_ref_refocus_map(cam_locations_[i], z, x, y, i);
        x.convertTo(xmap, CV_32FC1);
        y.convertTo(ymap, CV_32FC1);

        remap(imgs[i][0], res, xmap, ymap, INTER_LINEAR);

        refocused += res.clone();
        
    }

    cout<<"Time: "<<omp_get_wtime()-wall_timer<<endl;

    imshow("result", refocused); waitKey(0);

}

void saRefocus::calc_ref_refocus_map(Mat_<double> Xcam, double z, Mat_<double> &x, Mat_<double> &y, int cam) {

    int width = img_size_.width;
    int height = img_size_.height;

    Mat_<double> D = Mat_<double>::zeros(3,3);
    D(0,0) = scale_; D(1,1) = scale_;
    D(0,2) = width*0.5;
    D(1,2) = height*0.5;
    D(2,2) = 1;
    Mat Hinv = D.inv();

    Mat_<double> X = Mat_<double>::zeros(3, height*width);
    for (int i=0; i<width; i++) {
        for (int j=0; j<height; j++) {
            X(0,i*height+j) = i;
            X(1,i*height+j) = j;
            X(2,i*height+j) = 1;
        }
    }
    X = Hinv*X;

    for (int i=0; i<X.cols; i++)
        X(2,i) = z;

    cout<<"Refracting points"<<endl;
    Mat_<double> X_out = Mat_<double>::zeros(4, height*width);
    img_refrac(Xcam, X, X_out);

    cout<<"Projecting to find final map"<<endl;
    Mat_<double> proj = P_mats_[cam]*X_out;
    for (int i=0; i<width; i++) {
        for (int j=0; j<height; j++) {
            int ind = i*height+j; // TODO: check this indexing
            proj(0,ind) /= proj(2,ind);
            proj(1,ind) /= proj(2,ind);
            x(j,i) = proj(0,ind);
            y(j,i) = proj(1,ind);
        }
    }

}

void saRefocus::CPUrefocus(double z, double thresh, int live, int frame) {

    z *= warp_factor_;

    Scalar fact = Scalar(1/double(imgs.size()));

    Mat H, trans;
    T_from_P(P_mats_[0], H, z, scale_, img_size_);
    warpPerspective(imgs[0][frame], cputemp, H, img_size_);

    if (mult_) {
        pow(cputemp, mult_exp_, cputemp2);
    } else {
        multiply(cputemp, fact, cputemp2);
    }

    cpurefocused = cputemp2.clone();

    for (int i=1; i<num_cams_; i++) {
        
        T_from_P(P_mats_[i], H, z, scale_, img_size_);
        
        warpPerspective(imgs[i][frame], cputemp, H, img_size_);

        if (mult_) {
            pow(cputemp, mult_exp_, cputemp2);
            multiply(cpurefocused, cputemp2, cpurefocused);
        } else {
            multiply(cputemp, fact, cputemp2);
            add(cpurefocused, cputemp2, cpurefocused);        
        }
    }
    
    threshold(cpurefocused, cpurefocused, thresh, 0, THRESH_TOZERO);

    Mat refocused_host_(cpurefocused);
    //refocused_host_ /= 255.0;

    if (live) {
        refocused_host_ /= 255.0;
        char title[50];
        sprintf(title, "z = %f, thresh = %f, frame = %d", z/warp_factor_, thresh, frame);
        putText(refocused_host_, title, Point(10,20), FONT_HERSHEY_PLAIN, 1.0, Scalar(255,0,0));
        //line(refocused_host_, Point(646,482-5), Point(646,482+5), Scalar(255,0,0));
        //line(refocused_host_, Point(646-5,482), Point(646+5,482), Scalar(255,0,0));
        imshow("Result", refocused_host_);
    }

    refocused_host_.convertTo(result, CV_8U);

}

void saRefocus::img_refrac(Mat_<double> Xcam, Mat_<double> X, Mat_<double> &X_out) {

    double c[3];
    for (int i=0; i<3; i++)
        c[i] = Xcam.at<double>(0,i);

    for (int n=0; n<X.cols; n++) {

        double a[3];
        double b[3];
        double point[3];
        for (int i=0; i<3; i++)
            point[i] = X(i,n);

        a[0] = c[0] + (point[0]-c[0])*(zW_-c[2])/(point[2]-c[2]);
        a[1] = c[1] + (point[1]-c[1])*(zW_-c[2])/(point[2]-c[2]);
        a[2] = zW_;
        b[0] = c[0] + (point[0]-c[0])*(t_+zW_-c[2])/(point[2]-c[2]);
        b[1] = c[1] + (point[1]-c[1])*(t_+zW_-c[2])/(point[2]-c[2]);
        b[2] = t_+zW_;
        
        double rp = sqrt( pow(point[0]-c[0],2) + pow(point[1]-c[1],2) );
        double dp = point[2]-b[2];
        double phi = atan2(point[1]-c[1],point[0]-c[0]);

        double ra = sqrt( pow(a[0]-c[0],2) + pow(a[1]-c[1],2) );
        double rb = sqrt( pow(b[0]-c[0],2) + pow(b[1]-c[1],2) );
        double da = a[2]-c[2];
        double db = b[2]-a[2];
        
        double f, g, dfdra, dfdrb, dgdra, dgdrb;
        
        // Newton Raphson loop to solve for Snell's law
        double tol=1E-8;
        do {

            f = ( ra/sqrt(pow(ra,2)+pow(da,2)) ) - ( (n2_/n1_)*(rb-ra)/sqrt(pow(rb-ra,2)+pow(db,2)) );
            g = ( (rb-ra)/sqrt(pow(rb-ra,2)+pow(db,2)) ) - ( (n3_/n2_)*(rp-rb)/sqrt(pow(rp-rb,2)+pow(dp,2)) );
            
            dfdra = ( (1.0)/sqrt(pow(ra,2)+pow(da,2)) )
                - ( pow(ra,2)/pow(pow(ra,2)+pow(da,2),1.5) )
                + ( (n2_/n1_)/sqrt(pow(ra-rb,2)+pow(db,2)) )
                - ( (n2_/n1_)*(ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) );

            dfdrb = ( (n2_/n1_)*(ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
                - ( (n2_/n1_)/sqrt(pow(ra-rb,2)+pow(db,2)) );

            dgdra = ( (ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
                - ( (1.0)/sqrt(pow(ra-rb,2)+pow(db,2)) );

            dgdrb = ( (1.0)/sqrt(pow(ra-rb,2)+pow(db,2)) )
                + ( (n3_/n2_)/sqrt(pow(rb-rp,2)+pow(dp,2)) )
                - ( (ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
                - ( (n3_/n2_)*(rb-rp)*(2*rb-2*rp)/(2*pow(pow(rb-rp,2)+pow(dp,2),1.5)) );

            ra = ra - ( (f*dgdrb - g*dfdrb)/(dfdra*dgdrb - dfdrb*dgdra) );
            rb = rb - ( (g*dfdra - f*dgdra)/(dfdra*dgdrb - dfdrb*dgdra) );

        } while (f>tol || g >tol);
        
        a[0] = ra*cos(phi) + c[0];
        a[1] = ra*sin(phi) + c[1];

        X_out(0,n) = a[0];
        X_out(1,n) = a[1];
        X_out(2,n) = a[2];
        X_out(3,n) = 1.0;

    }

}
