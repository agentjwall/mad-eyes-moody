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

int const num_data = 19;

vector<int> to_int(vector<string> string_vector) {
    vector<int> int_vector;
    for (int x = 0; x < string_vector.size(); x++) {
        int_vector.push_back(stoi(string_vector[x]));
    }
    return int_vector;
}
/*
 * tests to see if the actual sphere is
 * the same as expected sphere
 * return true if it's a hit, false if it's a miss
 *
 * returns: true or false
 */
bool is_hit(vector<int> data) {
    assert(data.size() == 6);

    if (data[0] == data[2] && data[1] == data[3]) {
        return true;
    }
    return false;
}


/*
 * tests to see if the x value of the actual sphere
 * is the same as the x value of the expected sphere
 * return true if it's a hit, false if it's a miss
 *
 * returns: true or false
 */
bool is_x_hit(vector<int> data) {
    assert(data.size() == 6);

    if (data[0] == data[2]) {
        return true;
    }
    return false;
}

/*
 * tests to see if the y value of the actual sphere
 * is the same as the y value of the expected sphere
 * return true if it's a hit, false if it's a miss
 *
 * returns: true or false
 */
bool is_y_hit(vector<int> data) {
    assert(data.size() == 6);

    if (data[1] == data[3]) {
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {

    ifstream myfile("percent_error_data2.txt");
    string line;
    int hits = 0;
    int misses = 0;

    int hits_x = 0;
    int misses_x = 0;

    int hits_y = 0;
    int misses_y = 0;

    int sum_x = 0;
    int sum_y = 0;

    int count = 0;
    string line_number = "0";
    Point expected;

    while (getline(myfile, line)) {
        if (line[0] != 'R') {
            vector<string> data = split(line, ',');
            vector<int> numbers = to_int(data);
            expected = Point(numbers[0], numbers[1]);

            sum_x += numbers[4];
            sum_y += numbers[5];

            count++;

            if(is_hit(numbers)) {
                hits++;
            }
            else {
                misses++;
            }

            if (is_x_hit(numbers)) {
                hits_x++;
            }
            else {
                misses_x++;
            }

            if (is_y_hit(numbers)) {
                hits_y++;
            }
            else {
                misses_y++;
            }
        }
        //reset
        else {
            if (count == num_data) {
                cout << "regions " << line_number << " data" << endl;
                cout << "hits: " << hits << " misses: " << misses << " PERCENT ERROR: " << endl;
                cout << "hits on x-axis: " << hits_x << " misses on x-axis: " << misses_x << endl;
                cout << "hits on y-axis: " << hits_y << " misses on y-axis: " << misses_y << endl;
                cout << "average guess point: " << sum_x/num_data << ", " << sum_y/num_data << endl;
                cout << "expected point: " << expected.x << ", " << expected.y << endl;

                hits = 0;
                misses = 0;
                hits_x = 0;
                misses_x = 0;
                hits_y = 0;
                misses_y = 0;
                sum_x = 0;
                sum_y = 0;
                count = 0;
            }
            line_number = line[line.size() - 1];
        }


    }
    cout << "regions " << line_number << " data" << endl;
    cout << "hits: " << hits << " misses: " << misses << endl;
    cout << "hits on x-axis: " << hits_x << " misses on x-axis: " << misses_x << endl;
    cout << "hits on y-axis: " << hits_y << " misses on y-axis: " << misses_y << endl;
    cout << "average guess point: " << sum_x/num_data << ", " << sum_y/num_data << endl;
    cout << "expected point: " << expected.x << ", " << expected.y << endl;

    myfile.close();
    return 0;
}