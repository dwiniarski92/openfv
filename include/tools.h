#ifndef TOOLS_LIBRARY
#define TOOLS_LIBRARY

#include "std_include.h"
#include "typedefs.h"

using namespace std;
using namespace cv;

void init(int argc, char** argv);

void T_from_P(Mat P, Mat &H, double z, double scale, Size img_size);

bool dirExists(string dirPath);

int matrixMean(vector<Mat> mats_in, Mat &mat_out);

Mat P_from_KRT(Mat K, Mat rvec, Mat tvec, Mat rmean, Mat &P_u, Mat &P);

double dist(Point3f p1, Point3f p2);

void qimshow(Mat image);

void qimshow2(vector<Mat> imgs);

void pimshow(Mat image, double z, int n);

Mat getRotMat(double x, double y, double z);

void failureFunction();

void writeMat(Mat M, string path);

Mat getTransform(vector<Point2f> src, vector<Point2f> dst);

void listDir(string, vector<string> &);

void readImgStack(vector<string>, vector<Mat> &);

vector<double> linspace(double, double, int);

Mat cross(Mat_<double>, Mat_<double>);

Mat normalize(Mat_<double>);

// File IO class

class fileIO {

 public:
    ~fileIO() {
        LOG(INFO)<<"Closing file...";
        file.close(); 
    }

    fileIO(string filename);

    // TODO: add more templates
    void operator<< (int);
    void operator<< (float);
    void operator<< (double);
    void operator<< (string);
    void operator<< (const char*);
    void operator<< (vector<double>);
    
    /*
    void write(Mat);
    void write(vector<int>);
    void write(vector<float>);
    void write(vector<double>);
    void write(vector< vector<int> >);
    void write(vector< vector<float> >);
    void write(vector< vector<double> >);
    */

 protected:

 private:

    string getFilename(string filename);

    ofstream file;

};

#endif
