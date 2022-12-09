https://www.youtube.com/watch?v=0si2kZQkLsc&ab_channel=KelvinFilyk

This project performs high speed variable-K-means quantization on an input image. It is the culmination of my learnings in advanced C++ coursework.

Multi-Threaded K-Means Quantization Using Histogram of Unique Pixel Values

Let $TOP_DIR denote the directory containing this README file.
Let $INSTALL_DIR denote the directory into which this
software is to be installed.
To build and install the software, use the commands:

    cd $TOP_DIR

    cmake -S . -B $INSTALL_DIR

    cmake --build $INSTALL_DIR --clean-first

To run a demonstration, use the following commands:

    ./$INSTALL_DIR/quantize_image ./images/starry_night.jpeg 4

A new file 'starry_night_quantized_4.png' will be created in the images folder.
