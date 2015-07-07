#include <random>

//STL
#include <vector>

//Eigen
#include <Eigen/Eigen>

//Ceres
#include <ceres/ceres.h>
#include <ceres/rotation.h>

#include "geometry.h"
#include "matcher.h"
#include "vision.h"
#include "cartography.h"

using namespace ceres;
using Eigen::Matrix;

typedef Eigen::Matrix<double, 6, 6> Matrix6d;
typedef Eigen::Matrix<double, 6, 1> Vector6d;

using Eigen::Matrix3d;
using Eigen::Vector3d;
using Eigen::Vector2d;

using Eigen::RowMajor;

int countCalls = 0;

inline double sinc(const double x)
{
    if (x==0)
        return 1;
    return std::sin(x)/x;
}

OdometryError::OdometryError(const Vector3d X, const Vector2d pt,
        const Transformation<double> & TbaseCam,
        const ICamera & camera)
        : X(X), u(pt[0]), v(pt[1]), camera(&camera)
{
    TbaseCam.toRotTransInv(RcamBase, PcamBase);
}

ReprojectionErrorStereo::ReprojectionErrorStereo(const Vector2d pt,
        const Transformation<double> & TbaseCam,
        const ICamera * camera)
        : u(pt[0]), v(pt[1]), camera(camera)
{
    TbaseCam.toRotTransInv(RcamBase, PcamBase);
}

ReprojectionErrorFixed::ReprojectionErrorFixed(const Vector2d pt,
        const Transformation<double> & TorigBase,
        const Transformation<double> & TbaseCam, const ICamera * camera)
        : u(pt[0]), v(pt[1]), camera(camera)
{
    TorigBase.toRotTransInv(RbaseOrig, PbaseOrig);
    TbaseCam.toRotTransInv(RcamBase, PcamBase);
}

bool ReprojectionErrorFixed::Evaluate(double const* const* args,
                    double* residuals,
                    double** jac) const
{
    double newPoint[3];
    double rotationVec[3];
    const double * const landmark = args[0];
    Vector3d X(landmark);

    X = RcamBase*(RbaseOrig*X + PbaseOrig) + PcamBase;
    Vector2d point;
    camera->projectPoint(X, point);
    residuals[0] = point[0] - u;
    residuals[1] = point[1] - v;

    if (jac)
    {

        Eigen::Matrix<double, 2, 3> J;
        camera->projectionJacobian(X, J);

        // dp / dX
        Eigen::Matrix<double, 2, 3, RowMajor> dpdX = J * RcamBase * RbaseOrig;
        copy(dpdX.data(), dpdX.data() + 6, jac[0]);

    }

    return true;
}


//TODO unify the transformation system
bool OdometryError::Evaluate(double const* const* args,
                    double* residuals,
                    double** jac) const
{
    Vector3d rotOrigBase(args[1]);
    Matrix3d RbaseOrig = rotationMatrix<double>(-rotOrigBase);
    Vector3d PorigBase(args[0]);

    Vector3d Xtr = RcamBase * (RbaseOrig * (X - PorigBase)) + PcamBase;
    Vector2d point;
    camera->projectPoint(Xtr, point);
    residuals[0] = point[0] - u;
    residuals[1] = point[1] - v;

    if (jac)
    {

        Eigen::Matrix<double, 2, 3> J;
        camera->projectionJacobian(Xtr, J);

        Matrix3d Rco = RcamBase * RbaseOrig;


        // dp / dxi
        Eigen::Matrix<double, 3, 3> LxiInv;

        double theta = rotOrigBase.norm();
        if ( theta != 0)
        {
            Matrix3d uhat = hat<double>(rotOrigBase / theta);
            LxiInv = Matrix3d::Identity() +
                theta/2*sinc(theta/2)*uhat +
                (1 - sinc(theta))*uhat*uhat;
        }
        else
        {
            LxiInv = Matrix3d::Identity();
        }

        Eigen::Matrix<double, 2, 3, RowMajor> dpdxi1 = -J * Rco;
        Eigen::Matrix<double, 2, 3, RowMajor>  dpdxi2; // = (Eigen::Matrix<double, 2, 3, RowMajor> *) jac[2];
        dpdxi2 = J*hat(Xtr)*Rco*LxiInv;
        copy(dpdxi1.data(), dpdxi1.data() + 6, jac[0]);
        copy(dpdxi2.data(), dpdxi2.data() + 6, jac[1]);
    }

    return true;
}


bool ReprojectionErrorStereo::Evaluate(double const* const* args,
                    double* residuals,
                    double** jac) const
{
    Vector3d rot(args[2]);
    Matrix3d RbaseOrig = rotationMatrix<double>(-rot);
    Vector3d Pob(args[1]);
    Vector3d X(args[0]);

    X = RcamBase * (RbaseOrig * (X - Pob)) + PcamBase;
    Vector2d point;
    camera->projectPoint(X, point);
    residuals[0] = point[0] - u;
    residuals[1] = point[1] - v;

    if (jac)
    {

        Eigen::Matrix<double, 2, 3> J;
        camera->projectionJacobian(X, J);

        Matrix3d Rco = RcamBase * RbaseOrig;

        // dp / dX
        Eigen::Matrix<double, 2, 3, RowMajor> dpdX = J * Rco;
        copy(dpdX.data(), dpdX.data() + 6, jac[0]);

        // dp / dxi
        Eigen::Matrix<double, 3, 3> LxiInv;

        double theta = rot.norm();
        if ( theta != 0)
        {
            Matrix3d uhat = hat<double>(rot / theta);
            LxiInv = Matrix3d::Identity() +
                theta/2*sinc(theta/2)*uhat +
                (1 - sinc(theta))*uhat*uhat;
        }
        else
        {
            LxiInv = Matrix3d::Identity();
        }

        Eigen::Matrix<double, 2, 3, RowMajor>  dpdxi2; // = (Eigen::Matrix<double, 2, 3, RowMajor> *) jac[2];
        dpdX *= -1;
        dpdxi2 = J*hat(X)*Rco*LxiInv;
        copy(dpdX.data(), dpdX.data() + 6, jac[1]);
        copy(dpdxi2.data(), dpdxi2.data() + 6, jac[2]);
    }

    return true;
}

void MapInitializer::addFixedObservation(Vector3d & X, Vector2d pt, Transformation<double> & pose,
        const ICamera * cam, const Transformation<double> & TbaseCam)
{
    CostFunction * costFunc = new ReprojectionErrorFixed(pt, pose, TbaseCam, cam);
    problem.AddResidualBlock(costFunc, NULL, X.data());
}

void MapInitializer::addObservation(Vector3d & X, Vector2d pt, Transformation<double> & pose,
        const ICamera * cam, const Transformation<double> & TbaseCam)
{
    CostFunction * costFunc = new ReprojectionErrorStereo(pt, TbaseCam, cam);
    problem.AddResidualBlock(costFunc, NULL, X.data(), pose.transData(), pose.rotData());
}

void MapInitializer::compute()
{
    Solver::Options options;
    options.linear_solver_type = ceres::DENSE_SCHUR;
//    options.function_tolerance = 1e-3;
//    options.gradient_tolerance = 1e-4;
//    options.parameter_tolerance = 1e-4;
//    options.minimizer_progress_to_stdout = true;
    Solver::Summary summary;
    Solve(options, &problem, &summary);
//    cout << summary.FullReport() << endl;
}

void StereoCartography::projectPointCloud(const vector<Vector3d> & src,
        vector<Vector2d> & dst1, vector<Vector2d> & dst2, int poseIdx) const
{
    dst1.resize(src.size());
    dst2.resize(src.size());
    vector<Vector3d> Xb(src.size());
    trajectory[poseIdx].inverseTransform(src, Xb);
    stereo.projectPointCloud(Xb, dst1, dst2);
}

void StereoCartography::improveTheMap(bool firstBA)
{
    //BUNDLE ADJUSTMENT
    int lastFixedPos;
    if (firstBA)
    {
        lastFixedPos = 0;
    }
    else
    {
        int step = trajectory.size()-1;
        lastFixedPos = max(1, step - 4);
    }
    if (WM.size() > 10)
    {
        /*int fixedPos = trajectory.size();
        for (auto & landmark : WM)
        {
            int firstOb = landmark.observations[0].poseIdx;
            if (firstOb < fixedPos)
            {
                fixedPos = firstOb;
            }
        }*/
        MapInitializer initializer;
        for (auto & landmark : WM)
        {
            for (auto & observation : landmark.observations)
            {
                int xiIdx = observation.poseIdx;
                if (xiIdx <= lastFixedPos)
                {
                    if (observation.cameraId == LEFT)
                    {
                        initializer.addFixedObservation(landmark.X, observation.pt,
                                trajectory[xiIdx], stereo.cam1, stereo.TbaseCam1);
                    }
                    else
                    {
                        initializer.addFixedObservation(landmark.X, observation.pt,
                                trajectory[xiIdx], stereo.cam2, stereo.TbaseCam2);
                    }
                }
                else if (observation.cameraId == LEFT)
                {
                    initializer.addObservation(landmark.X, observation.pt,
                            trajectory[xiIdx], stereo.cam1, stereo.TbaseCam1);
                }
                else
                {
                    initializer.addObservation(landmark.X, observation.pt,
                            trajectory[xiIdx], stereo.cam2, stereo.TbaseCam2);
                }
            }
        }
        initializer.compute();
    }

}

void StereoCartography::improveTheMap_2()
{
    //BUNDLE ADJUSTMENT
    MapInitializer initializer;
    for (auto & landmark : WM)
    {
        for (auto & observation : landmark.observations)
        {
            int xiIdx = observation.poseIdx;
            if (xiIdx == 0)
            {
                if (observation.cameraId == LEFT)
                {
                    initializer.addFixedObservation(landmark.X, observation.pt,
                            trajectory[xiIdx], stereo.cam1, stereo.TbaseCam1);
                }
                else
                {
                    initializer.addFixedObservation(landmark.X, observation.pt,
                            trajectory[xiIdx], stereo.cam2, stereo.TbaseCam2);
                }
            }
            else if (observation.cameraId == LEFT)
            {
                initializer.addObservation(landmark.X, observation.pt,
                        trajectory[xiIdx], stereo.cam1, stereo.TbaseCam1);
            }
            else
            {
                initializer.addObservation(landmark.X, observation.pt,
                        trajectory[xiIdx], stereo.cam2, stereo.TbaseCam2);
            }
        }
    }
    initializer.compute();

}

void Odometry::computeTransformation()
{
    assert(observationVec.size() == cloud.size());
    assert(observationVec.size() == inlierMask.size());
    Problem problem;
    for (unsigned int i = 0; i < cloud.size(); i++)
    {

        if (not inlierMask[i]) continue;
        CostFunction * costFunc = new OdometryError(cloud[i],
                                        observationVec[i], TbaseCam, camera);
        problem.AddResidualBlock(costFunc, NULL,
                    TorigBase.transData(), TorigBase.rotData());
    }

    Solver::Options options;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    Solver::Summary summary;
    Solve(options, &problem, &summary);
}

void Odometry::computeTransformation_2()
{
    assert(observationVec_2.size() == cloud.size());
    assert(observationVec_2.size() == inlierMask_2.size());
    Problem problem;
    for (unsigned int i = 0; i < cloud.size(); i++)
    {
        for (int j = 0; j < inlierMask_2[i].size(); j++)
        {
            if (not inlierMask_2[i][j]) continue;
            CostFunction * costFunc = new OdometryError(cloud[i],
                                        observationVec_2[i][j], TbaseCam, camera);
            problem.AddResidualBlock(costFunc, NULL,
                                        TorigBase.transData(), TorigBase.rotData());
        }
    }

    Solver::Options options;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    Solver::Summary summary;
    Solve(options, &problem, &summary);
}

bool Odometry::checkSpan(vector<Vector3d> & ransacHP, double angleTh)
{
    assert(ransacHP.size() == 3);

    Transformation<double> tC = TorigBase.compose(TbaseCam);
    vector<Vector3d> HP;
    tC.inverseTransform(ransacHP, HP);
    double theta1 = std::acos((HP[0].dot(HP[1]))/(HP[0].norm()*HP[1].norm()));
    double theta2 = std::acos((HP[0].dot(HP[2]))/(HP[0].norm()*HP[2].norm()));
    double theta3 = std::acos((HP[1].dot(HP[2]))/(HP[1].norm()*HP[2].norm()));

    bool result = (std::abs(theta1-theta2) >= angleTh and
                   std::abs(theta1-theta3) >= angleTh and
                   std::abs(theta2-theta3) >= angleTh);
    return result;
}


void Odometry::Ransac()
{
    assert(observationVec.size() == cloud.size());
    int numPoints = observationVec.size();

    inlierMask.resize(numPoints);

    const int numIterMax = 300;
    const Transformation<double> initialPose = TorigBase;
    int bestInliers = 0;
    //TODO add a termination criterion
    for (unsigned int iteration = 0; iteration < numIterMax; iteration++)
    {
        Transformation<double> pose = initialPose;
        int maxIdx = observationVec.size();

        vector<Vector3d> ransacHP;
        int idx1m, idx2m, idx3m;
        int counter = 0;
        do
        {
            idx1m = rand() % maxIdx;

            do
            {
                idx2m = rand() % maxIdx;
            } while (idx2m == idx1m);

            do
            {
                idx3m = rand() % maxIdx;
            } while (idx3m == idx1m or idx3m == idx2m);

            ransacHP = { cloud[idx1m], cloud[idx2m], cloud[idx3m] };
            counter++;
            if (counter == 10000)
            {
                cout << endl << endl << "ERROR: Ransac could not find a model" << endl << endl;
                exit(1);
            }

        //} while (0);
        } while (checkSpan(ransacHP, 0.2) == false);


        //solve an optimization problem

        Problem problem;
        for (auto i : {idx1m, idx2m, idx3m})
        {
            CostFunction * costFunc = new OdometryError(cloud[i],
                                        observationVec[i], TbaseCam, camera);
            problem.AddResidualBlock(costFunc, NULL,
                        pose.transData(), pose.rotData());
        }

        Solver::Options options;
        options.linear_solver_type = ceres::DENSE_SCHUR;
        Solver::Summary summary;
        options.max_num_iterations = 10;
        Solve(options, &problem, &summary);

        //count inliers
        vector<Vector3d> XcamVec(numPoints);
        Transformation<double> TorigCam = pose.compose(TbaseCam);
        TorigCam.inverseTransform(cloud, XcamVec);
        vector<Vector2d> projVec(numPoints);
        camera.projectPointCloud(XcamVec, projVec);
        vector<bool> currentInlierMask(numPoints, false);

        int countInliers = 0;
        for (unsigned int i = 0; i < numPoints; i++)
        {
            Vector2d err = observationVec[i] - projVec[i];
            if (err.norm() < 2)
            {
                currentInlierMask[i] = true;
                countInliers++;
            }
        }
        //keep the best hypothesis
        if (countInliers > bestInliers)
        {
            //TODO copy in a bettegit lor way
            inlierMask = currentInlierMask;
            bestInliers = countInliers;
            TorigBase = pose;
        }
    }
}

void Odometry::Ransac_2()
{
    assert(observationVec_2.size() == cloud.size());
    int numPoints = observationVec_2.size();

    inlierMask_2.resize(numPoints);

    const int numIterMax = 500;
    const Transformation<double> initialPose = TorigBase;
    int bestInliers = 0;
    //TODO add a termination criterion
    for (unsigned int iteration = 0; iteration < numIterMax; iteration++)
    {
        Transformation<double> pose = initialPose;
        int maxIdx = observationVec_2.size();
        //choose three points at random
        vector<Vector3d> ransacHP;
        int idx1m, idx2m, idx3m;
        int counter = 0;
        do
        {
            idx1m = rand() % maxIdx;

            do
            {
                idx2m = rand() % maxIdx;
            } while (idx2m == idx1m);

            do
            {
                idx3m = rand() % maxIdx;
            } while (idx3m == idx1m or idx3m == idx2m);

            ransacHP = { cloud[idx1m], cloud[idx2m], cloud[idx3m] };
            counter++;
            if (counter == 10000)
            {
                cout << endl << endl << "ERROR: Ransac could not find a model" << endl << endl;
                exit(1);
            }

        //} while (0);
        } while (checkSpan(ransacHP, 0.15) == false);

        int idx1p = rand() % observationVec_2[idx1m].size();
        int idx2p = rand() % observationVec_2[idx2m].size();
        int idx3p = rand() % observationVec_2[idx3m].size();

        int index1 [3] = { idx1m, idx2m, idx3m };
        int index2 [3] = { idx1p, idx2p, idx3p };

        //solve an optimization problem

        Problem problem;
        for (int i = 0; i < 3; i++)
        {
            CostFunction * costFunc = new OdometryError(cloud[index1[i]],
                                        observationVec_2[index1[i]][index2[i]], TbaseCam, camera);
            problem.AddResidualBlock(costFunc, NULL,
                        pose.transData(), pose.rotData());
        }

        Solver::Options options;
        options.linear_solver_type = ceres::DENSE_SCHUR;
        Solver::Summary summary;
        options.max_num_iterations = 10;
        Solve(options, &problem, &summary);

        // project cloud to cam using estimated new pose
        vector<Vector3d> XcamVec(numPoints);
        vector<Vector2d> projVec(numPoints);
        Transformation<double> TorigCam = pose.compose(TbaseCam);

        TorigCam.inverseTransform(cloud, XcamVec);
        camera.projectPointCloud(XcamVec, projVec);

        // initialize empty inlier mask
        vector<vector<bool> > currentInlierMask(numPoints);
        for (int i = 0; i < numPoints; i++)
        {
            vector<bool> vec(observationVec_2[i].size(), false);
            currentInlierMask[i] = vec;
        }

        // count inliers
        int countInliers = 0;
        for (unsigned int i = 0; i < numPoints; i++)
        {
            double errNorm = 1000000;
            int best;
            for (int j = 0; j < observationVec_2[i].size(); j++)
            {
                double errNormTemp = (observationVec_2[i][j] - projVec[i]).norm();
                if (errNormTemp < errNorm)
                {
                    errNorm = errNormTemp;
                    best = j;
                }
            }
            if (errNorm < 2)
            {
                currentInlierMask[i][best] = true;
                countInliers++;
            }
        }

        //keep the best hypothesis
        if (countInliers > bestInliers)
        {
            //TODO copy in a bettegit lor way
            inlierMask_2 = currentInlierMask;
            bestInliers = countInliers;
            TorigBase = pose;

            // debug
            extern bool odometryDebug;
            if (odometryDebug)
            {
                extern vector<Eigen::Vector2d> oD_inlierFeat, oD_outlierFeat;
                extern vector<Eigen::Vector3d> oD_modelLM, oD_inlierLM, oD_outlierLM;
                oD_modelLM.clear();
                oD_inlierLM.clear();
                oD_inlierFeat.clear();
                oD_outlierLM.clear();
                oD_outlierFeat.clear();
                for (int i = 0; i < observationVec_2.size(); i++)
                {
                    bool inlier = false;
                    for(int j = 0; j < observationVec_2[i].size(); j++)
                    {
                        if (inlierMask_2[i][j])
                        {
                            inlier = true;
                            bool model = false;
                            for (int k = 0; k < 3; k++)
                            {
                                if (index1[k] == i and index2[k] == j)
                                {
                                    model = true;
                                }
                            }
                            if (model)
                            {
                                oD_modelLM.push_back(cloud[i]);
                            }
                            else
                            {
                                oD_inlierLM.push_back(cloud[i]);
                                oD_inlierFeat.push_back(observationVec_2[i][j]);
                            }
                        }
                        else
                        {
                            oD_outlierFeat.push_back(observationVec_2[i][j]);
                        }
                    }
                    if (inlier == false)
                    {
                        oD_outlierLM.push_back(cloud[i]);
                    }
                }
            }
        }
    }
}

// Transformation<double> StereoCartography::estimateOdometry(const vector<Feature> & featureVec) const
// {
//     //Matching
//
//     int numLandmarks = LM.size();
//     int numActive = min(100, numLandmarks);
//     vector<Feature> lmFeatureVec;
// //    cout << "ca va" << endl;
//     for (unsigned int i = numLandmarks - numActive; i < numLandmarks; i++)
//     {
//         lmFeatureVec.push_back(Feature(Vector2d(0, 0), LM[i].d));
//     }
// //    cout << "ca va" << endl;
//     Matcher matcher;
//     vector<int> matchVec;
//     matcher.bruteForce(lmFeatureVec, featureVec, matchVec);
//
//     Odometry odometry(trajectory.back(), stereo.TbaseCam1, stereo.cam1);
// //    cout << "ca va" << endl;
//     for (unsigned int i = 0; i < numActive; i++)
//     {
//         const int match = matchVec[i];
//         if (match == -1) continue;
//         odometry.observationVec.push_back(featureVec[match].pt);
//         odometry.cloud.push_back(LM[numLandmarks  - numActive + i].X);
//     }
// //    cout << "cloud : " << odometry.cloud.size() << endl;
//     //RANSAC
//     odometry.Ransac();
// //    cout << odometry.TorigBase << endl;
//     //Final transformation computation
//     odometry.computeTransformation();
// //    cout << odometry.TorigBase << endl;
//     return odometry.TorigBase;
// }

// odometry with ransac based on fixed BF matches
Transformation<double> StereoCartography::estimateOdometry(const vector<Feature> & featureVec) const
{
    //Matching

    int nSTM = STM.size();
    int nWM = WM.size();
    int maxActive = 300;
    int numActive = 0;
    vector<int> indexVec;
    vector<Feature> lmFeatureVec;

    // add landmarks from WM
    int k = nWM;
    while (k > 0 and numActive < maxActive)
    {
        k--;
        Eigen::Vector3d Xb, Xc;
        trajectory.back().inverseTransform(WM[k].X, Xb);
        stereo.TbaseCam1.inverseTransform(Xb, Xc);
        if (Xc(2) > 0.5 and WM[k].observations.back().poseIdx == trajectory.size()-1)
        {
            lmFeatureVec.push_back(Feature(Vector2d(0, 0), WM[k].d));
            numActive++;
            indexVec.push_back(k);
        }
    }
    int nWMreprojected = numActive;

    // add landmarks from STM
    k = nSTM;
    while (k > 0 and numActive < maxActive)
    {
        k--;
        Eigen::Vector3d Xb, Xc;
        trajectory.back().inverseTransform(STM[k].X, Xb);
        stereo.TbaseCam1.inverseTransform(Xb, Xc);
        if (Xc(2) > 0.5 and STM[k].observations.back().poseIdx == trajectory.size()-1)
        {
            lmFeatureVec.push_back(Feature(Vector2d(0, 0), STM[k].d));
            numActive++;
            indexVec.push_back(k);
        }
    }

    // matching
    vector<int> matchVec;
    matcher.bruteForceOneToOne(lmFeatureVec, featureVec, matchVec);

    Odometry odometry(trajectory.back(), stereo.TbaseCam1, stereo.cam1);

    for (int i = 0; i < nWMreprojected; i++)
    {
        if (matchVec[i] != -1)
        {
            odometry.observationVec.push_back(featureVec[matchVec[i]].pt);
            odometry.cloud.push_back(WM[indexVec[i]].X);
        }
    }
    for (int i = nWMreprojected; i < numActive; i++)
    {
        if (matchVec[i] != -1)
        {
            odometry.observationVec.push_back(featureVec[matchVec[i]].pt);
            odometry.cloud.push_back(STM[indexVec[i]].X);
        }
    }

//    cout << "cloud : " << odometry.cloud.size() << endl;
    //RANSAC
    odometry.Ransac();
//    cout << odometry.TorigBase << endl;
    //Final transformation computation
    odometry.computeTransformation();
//    cout << odometry.TorigBase << endl;
    return odometry.TorigBase;
}

// odometry based on motion hypothesis and reprojection matching
Transformation<double> StereoCartography::estimateOdometry_2(const vector<Feature> & featureVec) const
{
    int nSTM = STM.size();
    int nWM = WM.size();
    int maxActive = 300;
    int numActive = 0;

    // create motion hypothesis
    Transformation<double> Tdelta;
    if (trajectory.size() > 1)
    {
        Transformation<double> tn = trajectory[trajectory.size()-2].inverseCompose(trajectory.back());
        Tdelta.setParam(tn.trans(), tn.rot());
    }
    Transformation<double> Th = trajectory.back().compose(Tdelta);

    // predict position of the landmarks based on motion hypothesis

    vector<int> indexVec;
    vector<Feature> lmFeatureVec;
//    cout << "ca va" << endl;
    int k = nWM;
    while (k > 0 and numActive < maxActive)
    {
        k--;
        if (WM[k].observations.back().poseIdx == trajectory.size()-1)
        {
            Eigen::Vector3d Xb, Xc;
            Th.inverseTransform(WM[k].X, Xb);
            stereo.TbaseCam1.inverseTransform(Xb, Xc);
            if (Xc(2) > 0.5)
            {
                Eigen::Vector2d pos;
                bool res = stereo.cam1->projectPoint(Xc, pos);
                lmFeatureVec.push_back(Feature(pos, WM[k].d));
                numActive++;
                indexVec.push_back(k);
            }
        }
    }
    int nWMreprojected = lmFeatureVec.size();

    k = nSTM;
    while (k > 0 and numActive < maxActive)
    {
        k--;
        if (STM[k].observations.back().poseIdx == trajectory.size()-1)
        {
            Eigen::Vector3d Xb, Xc;
            Th.inverseTransform(STM[k].X, Xb);
            stereo.TbaseCam1.inverseTransform(Xb, Xc);
            if (Xc(2) > 0.5)
            {
                Eigen::Vector2d pos;
                bool res = stereo.cam1->projectPoint(Xc, pos);
                lmFeatureVec.push_back(Feature(pos, STM[k].d));
                numActive++;
                indexVec.push_back(k);
            }
        }
    }

    /*cout << endl << endl << "Tdelta: " << Tdelta << endl;
    cout << "Th: " << Th << endl;
    cout << "lmFeatureVec size: " << lmFeatureVec.size() << flush;*/

    vector<int> matchVec;
    matcher.matchReprojected(lmFeatureVec, featureVec, matchVec, 20);

    Odometry odometry(trajectory.back(), stereo.TbaseCam1, stereo.cam1);
//    cout << "ca va" << endl;
    for (int i = 0; i < nWMreprojected; i++)
    {
        if (matchVec[i] != -1)
        {
            odometry.observationVec.push_back(featureVec[matchVec[i]].pt);
            odometry.cloud.push_back(WM[indexVec[i]].X);
        }
    }
    for (int i = nWMreprojected; i < numActive; i++)
    {
        if (matchVec[i] != -1)
        {
            odometry.observationVec.push_back(featureVec[matchVec[i]].pt);
            odometry.cloud.push_back(STM[indexVec[i]].X);
        }
    }
//    cout << "cloud : " << odometry.cloud.size() << endl;
    //RANSAC
    odometry.Ransac();
//    cout << odometry.TorigBase << endl;
    //Final transformation computation
    odometry.computeTransformation();
//    cout << odometry.TorigBase << endl;
    return odometry.TorigBase;
}

// odometry with ransac based on BF matches pool
Transformation<double> StereoCartography::estimateOdometry_3(const vector<Feature> & featureVec) const
{
    //Matching
    int nSTM = STM.size();
    int nWM = WM.size();
    int maxActive = 300;
    int numActive = 0;
    vector<int> indexVec;
    vector<Feature> lmFeatureVec;

    int k = nWM;
    while (k > 0 and numActive < maxActive)
    {
        k--;
        Eigen::Vector3d Xb, Xc;
        trajectory.back().inverseTransform(WM[k].X, Xb);
        stereo.TbaseCam1.inverseTransform(Xb, Xc);
        if (Xc(2) > 0 and WM[k].observations.back().poseIdx == trajectory.size()-1)
        {
            lmFeatureVec.push_back(Feature(Vector2d(0, 0), WM[k].d));
            numActive++;
            indexVec.push_back(k);
        }
    }
    int nWMreprojected = numActive;

    if (WM.size() < 50)
    {
        k = nSTM;
        while (k > 0 and numActive < maxActive)
        {
            k--;
            Eigen::Vector3d Xb, Xc;
            trajectory.back().inverseTransform(STM[k].X, Xb);
            stereo.TbaseCam1.inverseTransform(Xb, Xc);
            if (Xc(2) > 0 and STM[k].observations.back().poseIdx == trajectory.size()-1)
            {
                lmFeatureVec.push_back(Feature(Vector2d(0, 0), STM[k].d));
                numActive++;
                indexVec.push_back(k);
            }
        }
    }
//    cout << "ca va" << endl;
    vector<vector<int> > matchVec;
    matcher.bruteForce_2(lmFeatureVec, featureVec, matchVec);

    Odometry odometry(trajectory.back(), stereo.TbaseCam1, stereo.cam1);
//    cout << "ca va" << endl;
    for (int i = 0; i < nWMreprojected; i++)
    {
        vector<Vector2d> vec;
        for (int j = 0; j < matchVec[i].size(); j++)
        {
            if (matchVec[i][j] != -1)
            {
                vec.push_back(featureVec[matchVec[i][j]].pt);
            }
        }
        if (vec.size() > 0)
        {
            odometry.observationVec_2.push_back(vec);
            odometry.cloud.push_back(WM[indexVec[i]].X);
        }
    }

    if (WM.size() < 50)
    {
        for (int i = nWMreprojected; i < numActive; i++)
        {
            vector<Vector2d> vec;
            for (int j = 0; j < matchVec[i].size(); j++)
            {
                if (matchVec[i][j] != -1)
                {
                    vec.push_back(featureVec[matchVec[i][j]].pt);
                }
            }
            if (vec.size() > 0)
            {
                odometry.observationVec_2.push_back(vec);
                odometry.cloud.push_back(STM[indexVec[i]].X);
            }
        }
    }

//    cout << "cloud : " << odometry.cloud.size() << endl;
    //RANSAC
    odometry.Ransac_2();
//    cout << odometry.TorigBase << endl;
    //Final transformation computation
    odometry.computeTransformation_2();
//    cout << odometry.TorigBase << endl;
    return odometry.TorigBase;
}
