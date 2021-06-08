/*
 * PoorMansHelper.cpp
 *
 *  Created on: Jul 3, 2014
 *      Author: Gerald
 */

#include "PoorMansHelper.h"

#include <iostream>
#include <fstream>
#include <iostream>
#include <sstream>





void PoorMansHelper::stackMatrices( Eigen::MatrixXd& a_A, const Eigen::MatrixXd a_B )
{
    Eigen::MatrixXd D(a_A.rows()+a_B.rows(), a_A.cols());
    D << a_A,
            a_B;

    a_A = D;
}

void PoorMansHelper::appendVectors( Eigen::VectorXd& a_A, const Eigen::VectorXd a_B )
{
    Eigen::VectorXd D(a_A.size()+a_B.size() );
    D << a_A,
            a_B;

    a_A = D;
}



Eigen::VectorXd PoorMansHelper::convertArrayToEigenVector ( float *a_vector, const int a_rows)
{
    Eigen::VectorXd VectorAsEigen;
    VectorAsEigen.setZero(a_rows);

    for(int i = 0; i < a_rows; ++i)
    {
        VectorAsEigen(i) = a_vector[i];
    }

    return VectorAsEigen;
}


Eigen::MatrixXd PoorMansHelper::convertArrayToEigenMatrix ( float **a_matrix, const int a_rows, const int a_cols )
{
    Eigen::MatrixXd MatrixAsEigen;
    MatrixAsEigen.setZero(a_rows, a_cols);

    for(int i = 0; i < a_rows; ++i)
    {
        for(int j = 0; j < a_cols; ++j) { MatrixAsEigen(i,j) = a_matrix[i][j]; }
    }

    return MatrixAsEigen;
}



void PoorMansHelper::writeMatrixToScreen(Eigen::MatrixXd& m)
{
    for(int i=0;i<m.rows();++i){
        for(int j=0;j<m.cols();++j){
            std::cout<<m(i,j);
            if(j!=m.cols()-1) std::cout<<',';
        }
        std::cout<<std::endl;
    }
    std::cout<<std::endl;
}





void PoorMansHelper::writeMatrixToCSV(Eigen::MatrixXd& m, const std::string& filename)
{

    std::ofstream file(filename,  std::ofstream::out | std::ofstream::trunc);
    if (file.is_open())
    {

        for(int i=0;i<m.rows();++i)
        {
            for(int j=0;j<m.cols();++j)
            {
                file << m(i,j);
                if(j!=m.cols()-1) file << ", ";
            }
            file << "\n";
        }
    }

    file.close();

}



void PoorMansHelper::readMatrixFromCSV(Eigen::MatrixXd &m, const std::string &filename)
{
    // ---------------------------------------------------------
    // Find number of rows and columns, and return to beginning
    // ---------------------------------------------------------

    int numRows = 0;
    int numCols = 0;
    std::string unused;
    std::ifstream file(filename);

    if (!file.is_open())
        std::cout << "PoorMansHelper:: Could not open CSV file: " << filename << std::endl;

    while ( std::getline(file, unused) ) // while not last line reached
    {
        if(0 == numRows) { // do this only once
            std::stringstream iss(unused);

            while(true) {
                std::string val;
                std::getline(iss, val, ',');
                ++numCols;

                if ( !iss.good() ) { // if we didn't reach end of line
                    break;
                }

            }
        }
        ++numRows;
    }

    //    std::cout << "numRows = " << numRows << std::endl;
    //    std::cout << "numRows = " << numCols << std::endl;

    file.clear();
    file.seekg(0, std::ios::beg);


    // -------------------------------------------------------
    // Read File into Matrix
    // -------------------------------------------------------

    m.resize(numRows, numCols);
    //    std::cout << "m = \n" << m << std::endl;

    double element;

    for(int row = 0; row < numRows; ++row)
    {
        std::string line;
        std::getline(file, line);
        if ( !file.good() )
            break;

        std::stringstream iss(line);

        for (int col = 0; col < numCols; ++col)
        {
            std::string val;
            std::getline(iss, val, ',');

            std::stringstream convertor(val);
            convertor >> element;
            m(row, col) = element;
        }
    }

}

void PoorMansHelper::recordToCSV(Eigen::VectorXd &v, const std::string &filename)
{

    std::ofstream file(filename,  std::ofstream::out | std::ofstream::app);
    if (file.is_open())
    {


        for(int i=0; i < v.rows(); ++i)
        {
            file << v(i);
            if(i!=v.rows()-1) file << ", ";
        }
        file << "\n";

    }

    //    file.close();
}



Eigen::Matrix3d PoorMansHelper::Quat2EigenMatrix(double &a_w, double &a_x, double &a_y, double &a_z)
{
    Eigen::Quaterniond quat_right_hand_state_local(a_w, a_x, a_y, a_z);
    Eigen::Matrix3d eig_mat;
    eig_mat = quat_right_hand_state_local.toRotationMatrix();
    return eig_mat;
}



Eigen::VectorXd PoorMansHelper::StringVec2EigenVec(const std::string &input)
{
    std::string input2 = input.substr(1, input.size() - 2); // remove []
    std::vector<double> res_vec;
    Eigen::VectorXd result;

    std::istringstream ss(input2);
    std::string token;

    while(std::getline(ss, token, ',')) {
        res_vec.push_back( std::stod (token) );
    }

    result = Eigen::VectorXd::Map(res_vec.data(), res_vec.size());
    return result;
}



Eigen::MatrixXd PoorMansHelper::StringMat2EigenMat(const std::string &input, int a_rows, int a_cols)
{
    std::string input2 = input.substr(1, input.size() - 2); // remove [ ]
    std::vector<double> res_vec;
    Eigen::MatrixXd result;
    result.resize(a_rows, a_cols);

    std::istringstream ss(input2);
    std::string token;

    while(std::getline(ss, token, ',')) {
        res_vec.push_back( std::stod (token) );
    }

    assert(a_rows*a_cols == res_vec.size() );

    for (int i=0; i < a_rows; ++i)
    {
        std::vector<double>::const_iterator first = res_vec.begin() + i*a_cols;
        std::vector<double>::const_iterator last  = res_vec.begin() + i*a_cols + a_cols;
        std::vector<double> newVec(first, last);
        result.row(i) = Eigen::VectorXd::Map(newVec.data(), newVec.size() );
    }
    return result;


    //    Eigen::Map<Eigen::MatrixXd> M(flat.data(), 3, 2);
    //    Eigen::MatrixXd M2(M.transpose());


}



std::string PoorMansHelper::EigenVec2String(Eigen::VectorXd &a_input)
{
    int size = a_input.size();
    std::string ret;
    ret.append("[");

    for (int i=0; i<size; ++i)
    {
        double el = a_input(i);
        ret.append( std::to_string(el) );

        if (i<size-1)
            ret.append( "," );
    }
    ret.append("]");
    return ret;
}





void PoorMansHelper::printConditionNumber(Eigen::MatrixXd &a_A, const std::string& a_name)
{
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(a_A);
    double cond = svd.singularValues()(0) / svd.singularValues()(svd.singularValues().size()-1);
    std::cout << "cond " << a_name << " = " << cond << std::endl;
    std::cout << "singularValues " << a_name << " = " << svd.singularValues() << std::endl;

}

Eigen::MatrixXd PoorMansHelper::svdInv(const Eigen::MatrixXd& M, const double epsilon /*= 0.01*/)
{
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(M, Eigen::ComputeFullU | Eigen::ComputeFullV);
    const Eigen::JacobiSVD<Eigen::MatrixXd>::SingularValuesType singVals = svd.singularValues();
    Eigen::JacobiSVD<Eigen::MatrixXd>::SingularValuesType invSingVals = singVals;
    for(int i=0; i<singVals.rows(); ++i) {
        if(singVals(i) <= epsilon) {
            invSingVals(i) = 1.0 / epsilon;
            //invSingVals(i) = 0.0;
        }
        else {
            invSingVals(i) = 1.0 / invSingVals(i);
        }
    }
    return Eigen::MatrixXd(svd.matrixV() *
                           invSingVals.asDiagonal() *
                           svd.matrixU().transpose());
}








Eigen::MatrixXd PoorMansHelper::svdInv2(const Eigen::MatrixXd &M, double epsilon)
{
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(M, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const auto &S = svd.singularValues();
    if (epsilon <= 0) {
        epsilon = std::numeric_limits<double>::epsilon() * std::max(M.cols(), M.rows()) * S(0);
    }
    return svd.matrixV() *
            (S.array() > epsilon).select(S.array().inverse(), 0).matrix().asDiagonal() *
            svd.matrixU().adjoint();

}




std::chrono::high_resolution_clock::time_point PoorMansHelper::startTimer() {
    auto start = std::chrono::high_resolution_clock::now();
    return start;
}

std::chrono::high_resolution_clock::time_point PoorMansHelper::endTimer(std::chrono::high_resolution_clock::time_point start, std::string output_text) {
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto elapsed_micro = elapsed.count();
    if (output_text == "") std::cout << "Elapsed time (in microseconds): "<< elapsed_micro << std::endl;
    else std::cout << output_text << " took " << elapsed_micro << " microseconds." << std::endl;
    return end;
}
