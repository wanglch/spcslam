#include <opencv2/opencv.hpp>
#include <Eigen/Eigen>
#include <iomanip>

#include <boost/format.hpp>

#include "geometry.h"
#include "vision.h"
#include "mei.h"
#include "calibration.h"

using namespace cv;
using Eigen::JacobiSVD;
int main(int argc, char** argv) {
    
    // Intrinsic calibration
    array<double, 6> params{0.5, 1, 500, 500, 500, 500};
//    array<double, 6> params{0.571, 1.18, 378.304,
//    377.959, 654.924, 474.837};

//    array<double, 6> params2{0.570, 1.181, 377.262,
//376.937, 659.913, 489.025};
    /*
    Intrinsic 1 : 
0.285248 0.165322 378.304
377.959 654.924 474.837
Intrinsic 2 : 
0.279888 0.170523 377.262
376.937 659.913 489.025
*/
    MeiCamera cam1(params.data());
    MeiCamera cam2(params.data());
    
    Transformation<double> xi(0, 0, 0, 0, 0, 0);
//    Transformation<double> xi(0.788018, 0.00458991, -0.0203444, -0.00243237, 0.0859827, 0.000373778);
    
    IntrinsicCameraCalibration<MeiProjector> calibLeft;
    IntrinsicCameraCalibration<MeiProjector> calibRight;
    
    if (calibLeft.initialize("calibInfo2.txt"))
    {
        calibLeft.compute(cam1);
        calibLeft.residualAnalysis(cam1);
    }
//    if (calibRight.initialize("calibInfoRight.txt"))
//    {
//        calibRight.compute(cam2);
//        calibRight.residualAnalysis(cam2);
//    }
    

    
//    ExtrinsicCameraCalibration<MeiProjector> calibStereo;
//    
////    if (calibStereo.initialize("calibInfoLeftSmall.txt","calibInfoRightSmall.txt", "calibInfoStereo.txt"))
//    if (calibStereo.initialize("calibInfoLeft.txt","calibInfoRight.txt", "calibInfoStereo.txt"))
//    {
//        calibStereo.compute(cam1, cam2, xi);
//        calibStereo.residualAnalysis(cam1, cam2, xi);
//    }
    
//    cout << "Extrinsic : " << endl;
//    cout << xi << endl;
//    
//    cout << "Intrinsic 1 : " << endl;
//    for (auto & p : cam1.params)
//    {
//        cout << boost::format("%3.3f ") % p;
//    }
//    cout << endl;
//    cout << "Intrinsic 2 : " << endl;
//    for (auto & p : cam2.params)
//    {
//        cout << boost::format("%3.3f ") % p;
//    }
//    cout << endl;
//    extrinsicStereoCalibration("calibInfoLeft.txt","calibInfoRight.txt", "calibInfoStereo.txt",
//            par1, par2, xi);
//    
//    cout << "Extrinsic : " << endl;
//    cout << xi << endl;
//    cout << "Intrinsic 1 : " << endl;
//    cout << par1[0] << " " << par1[1] << " " << par1[2] << endl;
//    cout << par1[3] << " " << par1[4] << " " << par1[5] << endl;
//    cout << "Intrinsic 2 : " << endl;
//    cout << par2[0] << " " << par2[1] << " " << par2[2] << endl;
//    cout << par2[3] << " " << par2[4] << " " << par2[5] << endl;
//    
    
    // Extrinsic calibration using epipolar geometry
    
    /*MeiCamera cam1mei(1296, 966, 0.579728, 1.13265, 372.661, 372.312, 655.471, 473.135);
//par1[0], par1[1], par1[2], par1[3], par1[4], par1[5]);
    MeiCamera cam2mei(1296, 966, 0.567528, 1.18243, 376.884, 376.175, 659., 488.023);
//par2[0], par2[1], par2[2], par2[3], par2[4], par2[5]);
    
    string fileName1 = "/home/bogdan/projects/icars/calib_images/13032015_stereo_zoe/left_1426238819.pgm";
    string fileName2 = "/home/bogdan/projects/icars/calib_images/13032015_stereo_zoe/left_1426238820.pgm";
    Mat frame1 = imread(fileName1, 0);
    Mat frame2 = imread(fileName2, 0);
    vector<Point2f> centers1, centers2;
    Size patternSize(8, 5);
    bool patternIsFound1 = findChessboardCorners(frame1, patternSize, centers1);
    bool patternIsFound2 = findChessboardCorners(frame2, patternSize, centers2);
    vector<Vector2d> proj1;
    for (auto & pt : centers1)
>>>>>>> calibration
    {
        proj1.push_back(Vector2d(pt.x, pt.y));
    }
    for (auto & pt : centers2)
    {
        proj1.push_back(Vector2d(pt.x, pt.y));
    }
<<<<<<< HEAD


    // Set up the only cost function (also known as residual). This uses
    // numeric differentiation to obtain the derivative (jacobian).
    CostFunction* cost_function =
    new NumericDiffCostFunction<BoardProjection, CENTRAL, 2*Nx*Ny, 6, 6> (
        new BoardProjection(proj, orig)
    );

    problem.AddResidualBlock(cost_function, NULL, params, xi);
}

int main(int argc, char** argv) {

    testCartography();
    testMatching();

    /*
    //TRY CALIBRATION FUNCTIONS

    double params[6]{0, 0, 1000, 1000, 650, 450},
=======
    
    fileName1 = "/home/bogdan/projects/icars/calib_images/13032015_stereo_zoe/right_1426238819.pgm";
    fileName2 = "/home/bogdan/projects/icars/calib_images/13032015_stereo_zoe/right_1426238820.pgm";
    frame1 = imread(fileName1, 0);
    frame2 = imread(fileName2, 0);
    patternIsFound1 = findChessboardCorners(frame1, patternSize, centers1);
    patternIsFound2 = findChessboardCorners(frame2, patternSize, centers2);
    vector<Vector2d> proj2;
    for (auto & pt : centers1)
    {
        proj2.push_back(Vector2d(pt.x, pt.y));
    }
    for (auto & pt : centers2)
    {
        proj2.push_back(Vector2d(pt.x, pt.y));
    }
    
    
    vector<Vector3d> pointVec1, pointVec2;
    Matrix3d E;
    cam1mei.reconstructPointCloud(proj1, pointVec1);
    cam2mei.reconstructPointCloud(proj2, pointVec2);
    
    computeEssentialMatrix(pointVec1, pointVec2, E);
    
    JacobiSVD<Matrix3d> svd(E, Eigen::ComputeFullU | Eigen::ComputeFullV);
    cout << svd.singularValues() << endl;
    
    cout << pointVec1[5].transpose() * E * pointVec2[5] << endl;
    
    Matrix3d R90;
    Matrix3d Rm90;
    R90 << 0, -1, 0, 1, 0, 0, 0, 0, 1;
    Rm90 << 0, 1, 0, -1, 0, 0, 0, 0, 1;
    Matrix3d R = svd.matrixU() * Rm90 * svd.matrixV().transpose();
    cout << "new rotation : " << endl;
    cout << R << endl << endl;
    R = svd.matrixU() * R90 * svd.matrixV().transpose();
//    Quaternion newQ(R);
//    cout << newQ << endl;
    cout << R << endl;
    //cout << "original rotation : " << endl;
   // cout << T2.rotMat() << endl;
    cout << "left sing vecs : " << endl;
    cout << svd.matrixU() << endl;    
    
    /*double params[6]{0, 0, 1000, 1000, 650, 450}, 
>>>>>>> calibration
    xi1[6]{-0.5, -0.5, 1, 0, 0, 0},
    xi2[6]{-0.5, -0.5, 1, 0, 0, 0},
    xi3[6]{-0.5, -0.5, 1, 0, 0, 0},
    xi4[6]{-0.5, -0.5, 1, 0, 0, 0},
    xi5[6]{-0.5, -0.5, 1, 0, 0, 0},
    xi6[6]{-0.5, -0.5, 1, 0, 0, 0},
    xi7[6]{-0.5, -0.5, 1, 0, 0, 0};

    google::InitGoogleLogging(argv[0]);


    Problem problem;
<<<<<<< HEAD

    addResidual("/home/bogdan/projects/icars/calib_dataset/left_1426238811.pgm",
=======
    
    addResidual("/home/bogdan/projects/icars/calib_dataset/right_1426238811.pgm",
>>>>>>> calibration
            problem, params, xi1);
    addResidual("/home/bogdan/projects/icars/calib_dataset/right_1426238815.pgm",
            problem, params, xi2);
    addResidual("/home/bogdan/projects/icars/calib_dataset/right_1426238818.pgm",
            problem, params, xi3);
    addResidual("/home/bogdan/projects/icars/calib_dataset/right_1426238820.pgm",
            problem, params, xi4);
    addResidual("/home/bogdan/projects/icars/calib_dataset/right_1426238823.pgm",
            problem, params, xi5);
    addResidual("/home/bogdan/projects/icars/calib_dataset/right_1426238826.pgm",
            problem, params, xi6);

    Solver::Options options;
    options.max_num_iterations = 50000;
    options.function_tolerance = 1e-10;
    options.gradient_tolerance = 1e-10;
    options.parameter_tolerance = 1e-10;
    //options.minimizer_progress_to_stdout = true;
    Solver::Summary summary;
    Solve(options, &problem, &summary);
    cout << summary.BriefReport() << endl;
    for (unsigned int i = 0; i < 6; i++)
    {
        cout << params[i] << " ";
    }
    cout << endl;
    for (unsigned int i = 0; i < 6; i++)
    {
        cout << xi1[i] << " ";
    }
    cout << endl;
    for (unsigned int i = 0; i < 6; i++)
    {
        cout << xi2[i] << " ";
    }*/
    return 0;


//
//    imshow("corners", frame);
//    waitKey();
}


