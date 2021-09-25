// Kelvin Filyk
// V00810568
// SENG475 - K_Means Quantization Project

#include <chrono>
#include <complex>
#include <fstream>
#include <iostream>
#include <thread>
#include "../include/ra/quantization_tools.hpp"
#include <opencv2/opencv.hpp>
#include <filesystem>

using namespace std;
using namespace ra::quantization;
using namespace cv;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

int main(int argc, char *argv[]) {
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <image_path> <uint_k>" << std::endl;
        //throw std::runtime_error("USAGE: quantize_image <img_path>");
        return 1;
    }

    std::string image_path;
    std::string output_path;

    try {
        image_path = argv[1];
        filesystem::path p(image_path);
        output_path = p.parent_path().string() + '/' + p.stem().string() + "_quantized_" + argv[2] + ".png"; 
        //std::cout << "Out path: " << output_path; 
    } catch(...) {
        std::cerr << "Image at path not found. \n" << std::endl;
        return 1;
    }
    Mat img = imread(image_path, IMREAD_UNCHANGED); // get the image regardless of num. channels (greyscale, colour, or col+alpha)

    //double min, max;

    // convert to RGBA (4-channel) image 
    if(img.type() == 0) { // typical of jp2 satellite files
        std::cout<<"FLAG\n";
        cvtColor(img, img, COLOR_GRAY2BGRA); // convert to rgba / 8bit colours
        cvtColor(img, img, COLOR_BGRA2RGBA); // convert to rgba / 8bit colours
    } else if(img.type() == 2) { // typical of jp2 satellite files
        img = img/128;
        img.convertTo(img, CV_8UC4);
        //cvtColor(img, img, COLOR_GRAY2RGBA); // should have 4 channels now
    } else if(img.type() == 16) { // standard rgb (ex. jpg)
        cvtColor(img, img, COLOR_RGB2RGBA); // convert to rgba / 8bit colours
    }
    /*
    minMaxLoc(img, &min, &max);
    std::cout<<"MIN: "<<min<<'\n';
    std::cout<<"MAX: "<<max<<'\n';
    */
    Mat out = img.clone(); // array of size (rows x cols) containing each pixel's correspondence to one of k centers

    std::cout<<img.type()<<'\n';
    std::cout<<img.channels()<<'\n';

    if(img.empty()) {
        std::cerr << "Could not read the image: " << image_path << std::endl;
        return 1;
    }
    int img_size = img.rows * img.cols;
    int k;
    try {
        k = atoi(argv[2]); // number of colours
        if (k<1){
            std::cerr << "Usage: " << argv[0] << " <image_path> <(uint_k > 1)>" << std::endl;
            return 1;
        } else if (k >= img_size) {
            std::cerr << "K value exceeds size of image! Please choose a smaller k. " << std::endl;
            return 1;
        }
    } catch(...){
        std::cerr << "Usage: " << argv[0] << " <image_path> <(uint_k > 1)>" << std::endl;
        return 1;
    }

    auto t1 = high_resolution_clock::now();
    quantize_image(img, out, k);
    auto t2 = high_resolution_clock::now();
    duration<double, std::milli> ms_double = t2 - t1;
    std::cout << ms_double.count() << "ms\n";
    //std::cout << "Press \'s\' to save quantized image. Otherwise, press any key to continue. \n";

    //imshow("Display window", out);
    //int key = waitKey(0); // Wait for a keystroke in the window
    //if(key =='s') {
    //imwrite(output_path, out);
    //}
    imwrite(output_path, out);


    return 0;
}
