/*
    TermView: An image viewer in an ANSI terminal with 24-bit color support
    Copyright (C) 2016, StarBrilliant <m13253@hotmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

static void PrintHelp(std::string const& progname) {
    std::cout << "Usage: " << progname << " image [PAR]" << std::endl
        << std::endl
        << "Arguments:" << std::endl
        << "    image   the image to display" << std::endl
        << "    PAR     pixel aspect ratio [default 0.5]" << std::endl
        << std::endl;
}

/* http://entropymine.com/imageworsener/srgbformula/ */
static float sRGBToLinear(uchar x) {
    if(x <= (uchar) (0.04045 * 255)) {
        return x / (12.92 * 255);
    } else {
        return std::pow(x / (1.055 * 255) + 0.055, 2.4);
    }
}

static double LinearToSRGB(float x) {
    if(x <= 0) {
        return 0;
    } else if(x <= 0.0031308) {
        return x * (12.92 * 255);
    } else {
        return std::min(std::pow(x, 1/2.4)*(1.055*255) - (0.055*255), 255.0);
    }
}

static void GetScreenSize(size_t& cols, size_t& rows) {
    rows = 0;
    cols = 0;
    char const* str_rows = std::getenv("ROWS");
    char const* str_cols = std::getenv("COLUMNS");
    if(str_rows) {
        try {
            rows = std::stof(str_rows)*2;
        } catch(std::exception const&) {
        }
    }
    if(str_cols) {
        try {
            cols = std::stof(str_cols);
        } catch(std::exception const&) {
        }
    }
    if(rows != 0 && cols != 0) {
        return;
    }
    struct winsize screen_size;
    if(ioctl(1, TIOCGWINSZ, &screen_size) != -1) {
        if(screen_size.ws_row) {
            rows = screen_size.ws_row*2;
        }
        if(screen_size.ws_col) {
            cols = screen_size.ws_col;
        }
    }
    if(rows == 0) {
        rows = 48;
    }
    if(cols == 0) {
        cols = 80;
    }
}

static void DisplayImage(cv::Mat const& image, double par) {
    size_t cols, rows;
    GetScreenSize(cols, rows);
    double scale_x = (double) image.cols / cols / (par*2);
    double scale_y = (double) image.rows / rows;
    double scale = std::max(scale_x, scale_y);
    cv::Mat screen_image(rows, cols, CV_32FC3);
    cv::Mat map_x(rows, cols, CV_32FC1);
    cv::Mat map_y(rows, cols, CV_32FC1);
    double center_from_x = (double) (cols - 1) / 2;
    double center_from_y = (double) (rows - 1) / 2;
    double center_to_x = (double) (image.cols - 1) / 2;
    double center_to_y = (double) (image.rows - 1) / 2;
    for(size_t x = 0; x < cols;  ++x) {
        double to_x = (x - center_from_x) * scale * (par*2) + center_to_x;
        for(size_t y = 0; y < rows; ++y) {
            map_x.at<float>(y, x) = to_x;
        }
    }
    for(size_t y = 0; y < rows; ++y) {
        double to_y = (y - center_from_y) * scale + center_to_y;
        for(size_t x = 0; x < cols; ++x) {
            map_y.at<float>(y, x) = to_y;
        }
    }
    cv::remap(image, screen_image, map_x, map_y,
        scale > 1 && scale * (par*2) > 1 ?
            cv::INTER_AREA :
        scale < 0.5 && scale * (par*2) < 0.5 ?
            cv::INTER_LANCZOS4 :
            cv::INTER_CUBIC
    );
    std::cout << "\x1b[40m\x1b[2J";
    for(size_t y = 0; y < rows/2; ++y) {
        std::cout << "\x1b[" << (y+1) << "H";
        for(size_t x = 0; x < cols; ++x) {
            cv::Vec3f const& fg = screen_image.at<cv::Vec3f>(y*2+1, x);
            cv::Vec3f const& bg = screen_image.at<cv::Vec3f>(y*2, x);
            std::cout << std::fixed << std::setprecision(0)
                << "\x1b[38;2;"
                << LinearToSRGB(fg[2]) << ';'
                << LinearToSRGB(fg[1]) << ';'
                << LinearToSRGB(fg[0]) << 'm'
                << "\x1b[48;2;"
                << LinearToSRGB(bg[2]) << ';'
                << LinearToSRGB(bg[1]) << ';'
                << LinearToSRGB(bg[0]) << 'm'
                << "\xe2\x96\x84";
        }
    }
    std::cout << std::flush;
}

static void WindowOnResize(int) {
}

static void AppOnExit(int) {
    std::cout << "\n\x1b[0m\x1b[?25h" << std::flush;
    std::exit(0);
}

static void ViewImageFile(std::string const& filename, double par) {
    cv::Mat image;
    {
        cv::Mat image_8bit = cv::imread(filename);
        if(!image_8bit.data) {
            throw std::runtime_error("failed to open the image");
        }
        if(image_8bit.cols == 0 || image_8bit.rows == 0) {
            throw std::runtime_error("image is empty");
        }
        image_8bit.convertTo(image, CV_32FC3, 1/255.0);
        image.create(image_8bit.rows, image_8bit.cols, CV_32FC3);
        for(size_t y = 0; y < image_8bit.rows; ++y) {
            for(size_t x = 0; x < image_8bit.cols; ++x) {
                cv::Vec3b const& pixel_8bit = image_8bit.at<cv::Vec3b>(y, x);
                cv::Vec3f& pixel = image.at<cv::Vec3f>(y, x);
                pixel[0] = sRGBToLinear(pixel_8bit[0]);
                pixel[1] = sRGBToLinear(pixel_8bit[1]);
                pixel[2] = sRGBToLinear(pixel_8bit[2]);
            }
        }
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGWINCH);
    sigprocmask(SIG_BLOCK, &sigset, nullptr);
    sigemptyset(&sigset);

    {
        struct sigaction repaint_action;
        repaint_action.sa_handler = WindowOnResize;
        sigemptyset(&repaint_action.sa_mask);
        sigaddset(&repaint_action.sa_mask, SIGWINCH);
        repaint_action.sa_flags = 0;
        sigaction(SIGWINCH, &repaint_action, nullptr);
    }

    {
        struct sigaction exit_action;
        exit_action.sa_handler = AppOnExit;
        sigemptyset(&exit_action.sa_mask);
        exit_action.sa_flags = 0;
        sigaction(SIGTERM, &exit_action, nullptr);
        sigaction(SIGINT, &exit_action, nullptr);
    }

    std::cout << "\x1b[?25l\x1b[2J";

    for(;;) {
        DisplayImage(image, par);
        sigsuspend(&sigset);
    }
}

int main(int argc, char* argv[]) {
    if(argc != 2 && argc != 3) {
        PrintHelp(argv[0]);
        return 0;
    }
    std::string const& filename = argv[1];
    double par = 0.5;
    if(argc == 3) {
        try {
            par = std::stof(argv[2]);
        } catch(std::exception const&) {
            std::cerr << "Invalid PAR value: " << argv[2] << std::endl;
            return 1;
        }
    }
    try {
        ViewImageFile(filename, par);
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
