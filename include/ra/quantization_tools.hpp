// Kelvin Filyk
// V00810568
// SENG475 - K_Means Quantization Project

#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <string>
#include "thread_pool.hpp"
#include <opencv2/opencv.hpp>
#include <unordered_set>

using namespace ra::concurrency;
using namespace cv;
using ul = unsigned long;
using StdMutex = std::mutex;
using Lock = std::unique_lock<std::mutex>;
using Pixel = std::tuple<int, int, int, int>;
namespace ra::quantization {
    StdMutex cluster_centers_mu_;
    StdMutex new_cluster_centers_mu_;
    StdMutex unique_colours_mu_;

    // init with k unique colours from image.
    // these should have decent spacing relative to k value. If k is 255, then spacing is 1. if k is 2, spacing is 30?

    void init_cluster_centers(std::map<Pixel, int> &unique_colours, std::map<Pixel, int> &cluster_centers, int k) { // have to pass in a ref to cluster_centers; also favourable to pass ref to unique_clusters for performance
        std::map<Pixel, int>::iterator it;
        Pixel p = {0,0,0,0};
        cluster_centers[p] = 0; // set up k unique cluster centers
        p = {255,255,255,255};
        cluster_centers[p] = 0; // set up k unique cluster centers
        while(cluster_centers.size() < (ul) k) { // for each center
            int rand = std::rand() % unique_colours.size();
            it = unique_colours.begin();
            std::advance(it, rand);
            cluster_centers[it->first] = 0; // set up k unique cluster centers
        }
    }

    // Compute nearest cluster for every unique colour in image
    double compute_cluster(Pixel colour, std::map<Pixel, int> &unique_colours, std::map<Pixel, int> &cluster_centers, std::map<Pixel, Pixel> &new_cluster_centers) {

        int r = get<0>(colour);
        int g = get<1>(colour);
        int b = get<2>(colour);
        int a = get<3>(colour);
        Pixel cluster; // stores the tuple value of the associated cluster
        ul dist = 0; // distance to colour value
        ul min_dist = -1;
        std::map<Pixel, int>::iterator it2;
        for(it2 = cluster_centers.begin(); it2!= cluster_centers.end(); it2++){
            ul d = get<0>(it2->first) - r;
            dist = d*d;
            d = get<1>(it2->first) - g;
            dist += d*d;
            d = get<2>(it2->first) - b;
            dist += d*d;
            d = get<3>(it2->first) - a;
            dist += d*d;
            if(it2 == cluster_centers.begin()) { // if no previous center allocated
                cluster = it2->first;
                min_dist = dist;
            } else if (dist<min_dist) { // if this center is closer to this pixel than all other cluster_centers
                cluster = it2->first;
                min_dist = dist;
            }                    
        }
        int i = unique_colours[colour];
        cluster_centers_mu_.lock();
        cluster_centers[cluster] += i; // add the number of pixels in the unique colour to the cluster
        cluster_centers_mu_.unlock();

        // add r, g, b, a of unique colours associated to cluster to pre-existing tuple value
        new_cluster_centers_mu_.lock();
        get<0>(new_cluster_centers[cluster]) += r*i;
        get<1>(new_cluster_centers[cluster]) += g*i;
        get<2>(new_cluster_centers[cluster]) += b*i;
        get<3>(new_cluster_centers[cluster]) += a*i;     
        new_cluster_centers_mu_.unlock();
        return min_dist;
    }

    // Get all unique colours in the image
    void get_unique_colours(Mat img, std::map<Pixel, int> &unique_colours, int row, int cols) {
        Pixel p;
        for(int col = 0; col<cols; col++) {
            p = {img.data[row * cols * 4 + col * 4], img.data[row * cols * 4 + col * 4 + 1], img.data[row * cols * 4 + col * 4 + 2] ,img.data[row * cols * 4 + col * 4 + 3]};
            // insert only unique colours into map

            unique_colours_mu_.lock();
            if(unique_colours.contains(p)) {
                unique_colours[p]++;
            } else {
                unique_colours[p] = 1;
            }
            unique_colours_mu_.unlock();

        }
        //std::cout<<"DONE ROW "<<row<<"\n";
    }

    void quantize_image(Mat img, Mat out, int k) {
        int rows = img.rows;
        int cols = img.cols;
        int chans = img.channels();
        std::map<Pixel, int> cluster_centers; // array of k cluster_centers pertaining to 'chans' colour channels, 
        std::map<Pixel, Pixel> new_cluster_centers; // array of k cluster_center tuples pertaining to final pixel values after each k-means operation
        std::map<Pixel, int> unique_colours; // store every unique colour in image, plus number of pixel members

        thread_pool tp; // create thread pool with max possible num of threads for this hardware

        // alpha: 0 is transparent, 255 is opaque
        //Pixel p;
        for(int row = 0; row< rows; row++) {
            tp.schedule([&, row]() { // only pass 'row' by copy- pass all else by ref
                get_unique_colours(img, unique_colours, row, cols);
            });      
        }
        tp.block_until_idle();

        if((ul) k > unique_colours.size()) {
            std::cerr << "K value exceeds number of unique colours in image! Please choose a smaller k. " << std::endl;
            exit(EXIT_FAILURE);
        }

        std::map<Pixel, int>::iterator it;
        /*
        for (it = unique_colours.begin(); it != unique_colours.end(); it++) {
            //std::cout << get<0>(it->first) << " " << get<1>(it->first) << " " << get<2>(it->first) << " "<< get<3>(it->first) << ": "<< it->second << "\n"; // uncomment this line for a counted list of every unique colour 
        }
        */

        std::cout<<"("<<rows<<"x"<<cols<<"x"<<chans<<"): "<<unique_colours.size()<<" COLOURS \n";

        init_cluster_centers(unique_colours, cluster_centers, k);

        std::cout<<"INITIAL CLUSTER CENTERS: \n";
        for (it = cluster_centers.begin(); it != cluster_centers.end(); it++) {
            std::cout << get<0>(it->first) << " " << get<1>(it->first) << " " << get<2>(it->first) << " "<< get<3>(it->first) << "\n "; // get cluster center, count
        }

        // at this point we have n unique colours and k randomly initialized cluster centers
        ul prev_dist = -1; // first run
        ul iter_dist = -1;
        std::map<Pixel, Pixel>::iterator it3;
        ul negative_one = -1;
        //std::cout<<"FLAG\n";

        while( (prev_dist/iter_dist < 0.999999 && prev_dist/iter_dist > 1.000001) || iter_dist == negative_one || prev_dist == negative_one){ // continue iterating until acceptable
 
            if(iter_dist != negative_one) {
                prev_dist = iter_dist;
            }

            iter_dist = 0;
            
            // reset cluster_centers, clear new_cluster_centers 
            new_cluster_centers.clear(); 
            std::map<Pixel, int>::iterator it5;
            for (it5 = cluster_centers.begin(); it5 != cluster_centers.end(); it5++) {
                new_cluster_centers[it5->first] = {0,0,0,0}; // - this will hold the next iteration of cluster centers modified from the previous iteration
            }
            //std::cout<<"FLAG10\n";
            //go through every unique colour, and compute its distance from a cluster center. If it falls within that cluster, add all of its pixels to the pixel count of that cluster center.
            std::map<Pixel, int>::iterator it6 = unique_colours.begin();
            Pixel p2;
            for(ul i = 0; i< unique_colours.size(); i++) {
            //for (it = unique_colours.begin(); it != unique_colours.end(); it++) {
                p2 = it6->first;
                tp.schedule([&, p2]() { // pass 'p2' by copy- pass all else by ref
                    iter_dist += compute_cluster(p2, unique_colours, cluster_centers, new_cluster_centers);
                });  
                it6++;
                //std::cout<<"...\n";
            }
            tp.block_until_idle();
            // confirm all clusters have been calculated before moving on! Use block_until_idle to ensure all threads complete
            //std::cout<<"FLAG6\n";


            /*
            std::cout<<"NEW CLUSTER CENTERS: \n";
            for (it3 = new_cluster_centers.begin(); it3 != new_cluster_centers.end(); it3++) {
                std::cout << get<0>(it3->first) << " " << get<1>(it3->first) << " " << get<2>(it3->first) << " "<< get<3>(it3->first) << ": " << get<0>(it3->second) << " " << get<1>(it3->second) << " " << get<2>(it3->second) << " "<< get<3>(it3->second)  <<"\n"; // get cluster center, count
            }
            */

            // now compute next iteration of cluster centers
            //std::cout<<"CLUSTER CENTER COUNTS: \n";

            for (it3 = new_cluster_centers.begin(); it3 != new_cluster_centers.end(); it3++) { // iterate through new cluster center values
                Pixel old_center = it3->first; //save the Pixel value the old cluster
                int n = cluster_centers[old_center]; // get the total number of pixels within that cluster
                //std::cout << get<0>(old_center) << " " << get<1>(old_center) << " " << get<2>(old_center) << " "<< get<3>(old_center) << ": " << n << "\n";
                cluster_centers.erase(old_center); // get rid of old cluster
                //std::cout<<get<0>(it3->second)+n-1<<"/"<<(double)n<<"\n";
                //std::cout<<get<1>(it3->second)+n-1<<"/"<<(double)n<<"\n";
                //std::cout<<get<2>(it3->second)+n-1<<"/"<<(double)n<<"\n";
                //std::cout<<get<3>(it3->second)+n-1<<"/"<<(double)n<<"\n";

                int r = (double)(get<0>(it3->second)+n-1)/(double)n;
                int g = (double)(get<1>(it3->second)+n-1)/(double)n;
                int b = (double)(get<2>(it3->second)+n-1)/(double)n;
                int a = (double)(get<3>(it3->second)+n-1)/(double)n;
                //std::cout<<r<<"\n";
                //std::cout<<g<<"\n";
                //std::cout<<b<<"\n";
                //std::cout<<a<<"\n";

                Pixel new_center = {r, g, b, a}; // establish new cluster to replace old
                //std::cout<<"FLAG11\n";
                cluster_centers[new_center] = 0;
            }

            std::cout<<"ITERATING... "<<iter_dist<<"\n";
        }

        // after while loop is over, do one last computation to find which cluster each unique colour belongs to 
        std::map<Pixel, int>::iterator it2;
        std::map<Pixel, Pixel> unique_colour_clusters; // store every unique colour in image, plus number of pixel members
        for (it = unique_colours.begin(); it != unique_colours.end(); it++) {
            int r = get<0>(it->first);
            int g = get<1>(it->first);
            int b = get<2>(it->first);
            int a = get<3>(it->first);
            Pixel cluster; // stores the tuple value of the chosen cluster
            ul dist = 0; // distance to colour value
            ul min_dist = -1;
            for(it2 = cluster_centers.begin(); it2!= cluster_centers.end(); it2++){
                ul d = get<0>(it2->first) - r;
                dist = d*d;
                d = get<1>(it2->first) - g;
                dist += d*d;
                d = get<2>(it2->first) - b;
                dist += d*d;
                d = get<3>(it2->first) - a;
                dist += d*d;
                if(it2 == cluster_centers.begin()) { // if no previous center allocated
                    cluster = it2->first;
                    min_dist = dist;
                } else if (dist<min_dist) { // if this center is closer to this pixel than all other cluster_centers
                    cluster = it2->first;
                    min_dist = dist;
                }                    
            }
            // at this point, we have the final cluster this unique colour belongs to. add to unique_colour_clusters
            unique_colour_clusters[it->first] = cluster;
        }

        // write clusters to output
        for(int row = 0; row<rows; row++) {
            for(int col = 0; col<cols; col++) {
                // 
                Pixel o = {img.data[row * cols * 4 + col * 4], img.data[row * cols * 4 + col * 4 + 1], img.data[row * cols * 4 + col * 4 + 2], img.data[row * cols * 4 + col * 4 + 3]};
                // o is a unique colour
                o = unique_colour_clusters[o];
                // o is now a cluster colour

                out.data[row * cols * 4 + col * 4] = get<0>(o);
                out.data[row * cols * 4 + col * 4 + 1] = get<1>(o);
                out.data[row * cols * 4 + col * 4 + 2] = get<2>(o);   
                out.data[row * cols * 4 + col * 4 + 3] = get<3>(o);                    
            }
        }
        
        std::cout<<"FINAL CLUSTER CENTERS: \n";
        for (it = cluster_centers.begin(); it != cluster_centers.end(); it++) {
            std::cout << get<0>(it->first) << " " << get<1>(it->first) << " " << get<2>(it->first) << " "<< get<3>(it->first) << "\n"; // get cluster center, count
        }     
    }
}  // namespace ra::quantization