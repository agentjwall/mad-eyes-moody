#include <iostream>
#include "statistics.h"
#include <opencv2/objdetect/objdetect.hpp>
#include <numeric>
#include <fstream>

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

vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

int const num_data = 20;

int main(int argc, char* argv[]) {

    ifstream myfile("percent_error_data1.txt");
    string line;
    int hits = 0;
    int hits_x = 0;
    int misses = 0;
    int misses_x = 0;
    int hits_y = 0;
    int misses_y = 0;
    int sum_x = 0;
    int sum_y = 0;
    while (getline(myfile, line)) {
        if (line[0] != 'R') {
            vector<string> data = split(line, ',');
            for (int x = 0; x < data.size(); x++) {
                cout << data[x] << "-";
            }
            cout << endl;
        }
        //reset
        else {
            if (hits + misses + sum_x + sum_y > 0) {
                cout << "print percent error" << endl;
            }
        }
    }
    myfile.close();
    return 0;
}