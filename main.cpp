#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <complex>
#include <chrono>
#include <thread> 
#include <string>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <png.h>
#include <mutex>

using namespace std;
using namespace chrono;

constexpr unsigned UPSCALE = 4;

constexpr unsigned WIDTH = 2880 * UPSCALE;
constexpr unsigned HEIGTH = 1800 * UPSCALE;
constexpr png_byte BIT_DEPTH = 8;
constexpr png_byte COLOR_TYPE = PNG_COLOR_TYPE_RGBA;
constexpr unsigned MAX_ITER = 50;

constexpr double kinda_zoom = 1.0 / (0.5); // change value in parenthesis
constexpr double startx = -0.5 * kinda_zoom / 2.0;
constexpr double starty = 0 * kinda_zoom;

constexpr double W_FACTOR = (double)WIDTH / HEIGTH;

constexpr double MAX_RE = startx + W_FACTOR * kinda_zoom / 2;
constexpr double MIN_RE = startx - W_FACTOR * kinda_zoom / 2;
constexpr double MAX_IM = starty + kinda_zoom / 2;
constexpr double MIN_IM = starty - kinda_zoom / 2;

const char* FILENAME;

png_infop info_ptr;
png_structp png_ptr;
png_bytep* row_ptrs;

FILE* fp;

void create_png()
{
	fp = fopen(FILENAME, "wb");
	assert(fp != nullptr);

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	assert(png_ptr != nullptr);

	info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr != nullptr);

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGTH, BIT_DEPTH, COLOR_TYPE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	row_ptrs = (png_bytep*)malloc(sizeof(png_bytep) * HEIGTH);
	if (row_ptrs != nullptr)
	{
		for (size_t i = 0; i < HEIGTH; i++)
		{
			row_ptrs[i] = (png_bytep)malloc(sizeof(png_byte) * WIDTH * 4);
		}
	}
}

void write_and_close_png()
{
	png_write_image(png_ptr, row_ptrs);
	png_write_end(png_ptr, info_ptr);
	for (size_t i = 0; i < HEIGTH; i++)
	{
		free(row_ptrs[i]);
	}
	free(row_ptrs);
	fclose(fp);
}

void HSVtoRGB(int H, double S, double V, int output[3]) {
	double C = S * V;
	double X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
	double m = V - C;
	double Rs, Gs, Bs;

	if (H >= 0 && H < 60) {
		Rs = C;
		Gs = X;
		Bs = 0;
	}
	else if (H >= 60 && H < 120) {
		Rs = X;
		Gs = C;
		Bs = 0;
	}
	else if (H >= 120 && H < 180) {
		Rs = 0;
		Gs = C;
		Bs = X;
	}
	else if (H >= 180 && H < 240) {
		Rs = 0;
		Gs = X;
		Bs = C;
	}
	else if (H >= 240 && H < 300) {
		Rs = X;
		Gs = 0;
		Bs = C;
	}
	else {
		Rs = C;
		Gs = 0;
		Bs = X;
	}

	output[0] = (int)((Rs + m) * 255);
	output[1] = (int)((Gs + m) * 255);
	output[2] = (int)((Bs + m) * 255);
}

mutex _lock;

//double i_sum = 0;
void draw_mandelbrot(int y, int to_y)
{
	for (; y < to_y; y++)
	{
		for (size_t x = 0; x < WIDTH; x++)
		{
			complex<double> c;
			c = 1i * (MAX_IM - y * (MAX_IM - MIN_IM) / ((double)HEIGTH - 1));
			c += MIN_RE + x * (MAX_RE - MIN_RE) / ((double)WIDTH - 1);
			size_t i;
			complex<double> z;
			z += c;
			for (i = 0; i < MAX_ITER; i++)
			{
				if (abs(z) > 2.0)
				{
					break;
				}
				z *= z;
				z += c;
			}
			png_byte tmp[3];
			if (i != MAX_ITER)
			{
				//i_sum +=i;
				int hue = (int)(160 + 120 - 120 * pow((double)i / ((double)MAX_ITER - 1), 1.3));
				hue = clamp(hue, 0, 300);
				double val = pow((double)i / ((double)MAX_ITER - 1), 1.3);
				val = clamp(val, 0.0, 1.0);
				int hsvout[3];
				HSVtoRGB(hue, 1.0, val, hsvout);
				for (uint8_t chan = 0; chan < 3; chan++)
				{
					tmp[chan] = (png_byte)hsvout[chan];
				}
			}
			else
			{
				for (uint8_t chan = 0; chan < 3; chan++)
				{
					tmp[chan] = 0;
				}

			}

			_lock.lock();
			for (uint8_t chan = 0; chan < 3; chan++)
			{
				row_ptrs[y][x * 4 + chan] = tmp[chan];
			}

			row_ptrs[y][x * 4 + 3] = 255;
			_lock.unlock();
		}
	}
}

int main()
{
	FILENAME = ("mandelbrot_" + to_string(UPSCALE) + "x_" + to_string(WIDTH / UPSCALE) + "x" + 
				to_string(HEIGTH / UPSCALE) + "_" + to_string(MAX_ITER) + "iters.png").c_str();
	high_resolution_clock::time_point t_start = high_resolution_clock::now();
	create_png();
	unsigned max_threads = thread::hardware_concurrency();
	constexpr unsigned DIV_INTO = max(1u, HEIGTH * MAX_ITER / 1000 / 50);
	//unsigned max_threads = 1; // for test purposes
	vector<thread> threads(max_threads);
	unsigned prev = 0;
	for (size_t iteration = 0; iteration < DIV_INTO; iteration++)
	{
		for (size_t i = 0; i < max_threads; i++)
		{
			cerr << "Thread launched from " << prev << " up to " << prev + HEIGTH / max_threads / DIV_INTO << endl;
			threads[i] = thread(draw_mandelbrot, prev, prev + HEIGTH / max_threads / DIV_INTO);
			prev += HEIGTH / max_threads / DIV_INTO;
		}
		for (size_t i = 0; i < max_threads; i++)
		{
			threads[i].join();
		}
	}
	if (prev != HEIGTH)
	{
		cerr << "Thread launched from " << prev << " up to " << HEIGTH << endl;
		threads[0] = thread(draw_mandelbrot, prev, HEIGTH);
		threads[0].join();
	}
	high_resolution_clock::time_point t_end = high_resolution_clock::now();

	write_and_close_png();
	cout << "mandelbrot?" << endl;
	duration<double> t_span = duration_cast<duration<double>>(t_end - t_start);
	cout << "process time: " << t_span.count() << "s" << endl;
	//cout << "avg i is " << (i_sum) / (WIDTH*HEIGTH) << endl; // needs rewrite to work, considering threads sharing

	cin.ignore();
	cin.get();
	return 0;
}
