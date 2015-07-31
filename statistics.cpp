#include <iostream>
#include "statistics.h"
#include <opencv2/objdetect/objdetect.hpp>
#include <numeric>

using namespace cv;
using namespace std;

void percent_error(Point actual, Point measured, int pixels_x, int pixels_y) {
    double error_x = abs((actual.x/pixels_x) - (measured.x/pixels_x)) /
            (actual.x/pixels_x);
    double error_y = abs((actual.y/pixels_y) - (measured.y/pixels_y)) /
            (actual.y/pixels_y);
}

Point averge_distance(vector<Point> measured_points) {
    double sum_x, sum_y;
    for(int x = 0; x < measured_points.size(); x++) {
        sum_x += measured_points[x].x;
        sum_y += measured_points[x].y;
    }
    return Point(sum_x/measured_points.size(), sum_y/measured_points.size());
}

int main(int argc, char* argv[]) {

    return 0;
}