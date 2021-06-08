/*
 * PoorMansHelper.h
 *
 *  Created on: Jul 3, 2014
 *      Author: Gerald
 */

#ifndef POORMANSHELPER_H_
#define POORMANSHELPER_H_

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <chrono>
#include <vector>





namespace PoorMansHelper {



Eigen::VectorXd convertArrayToEigenVector ( float *a_vector, const int a_rows);
Eigen::MatrixXd convertArrayToEigenMatrix ( float **a_matrix, const int a_rows, const int a_cols );

void stackMatrices( Eigen::MatrixXd& a_A, const Eigen::MatrixXd a_B );
void appendVectors( Eigen::VectorXd& a_A, const Eigen::VectorXd a_B );

void writeMatrixToScreen(Eigen::MatrixXd& m);
void writeMatrixToCSV(Eigen::MatrixXd& m, const std::string& filename);
void readMatrixFromCSV(Eigen::MatrixXd& m, const std::string& filename);
void recordToCSV(Eigen::VectorXd& m, const std::string& filename);


Eigen::Matrix3d Quat2EigenMatrix(double& a_w, double& a_x, double& a_y, double& a_z);

Eigen::VectorXd StringVec2EigenVec(const std::string& input);
Eigen::MatrixXd StringMat2EigenMat(const std::string& input, int a_rows, int a_cols);
std::string EigenVec2String(Eigen::VectorXd& a_input);



void printConditionNumber(Eigen::MatrixXd& a_A, const std::string &a_name);

Eigen::MatrixXd svdInv(const Eigen::MatrixXd& M, const double epsilon = 0.01);

Eigen::MatrixXd svdInv2(const Eigen::MatrixXd& M, double epsilon = 0);

std::chrono::high_resolution_clock::time_point startTimer();
std::chrono::high_resolution_clock::time_point endTimer(std::chrono::high_resolution_clock::time_point start, std::string output_text = "");



} /* namespace PoorMansHelper */
#endif /* POORMANSHELPER_H_ */
