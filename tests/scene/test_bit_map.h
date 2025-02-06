/**************************************************************************/
/*  test_bit_map.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/os/memory.h"
#include "scene/resources/bit_map.h"
#include "tests/test_macros.h"

namespace TestBitmap {

void reset_bit_map(BitMap &p_bm) {
	Size2i size = p_bm.get_size();
	p_bm.set_bit_rect(Rect2i(0, 0, size.width, size.height), false);
}

TEST_CASE("[BitMap] Create bit map") {
	Size2i dim{ 256, 512 };
	BitMap bit_map{};
	bit_map.create(dim);
	CHECK(bit_map.get_size() == Size2i(256, 512));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "This will go through the entire bitmask inside of bitmap, thus hopefully checking if the bitmask was correctly set up.");

	ERR_PRINT_OFF

	dim = Size2i(0, 256);
	bit_map.create(dim);
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 512), "We should still have the same dimensions as before, because the new dimension is invalid.");

	dim = Size2i(512, 0);
	bit_map.create(dim);
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 512), "We should still have the same dimensions as before, because the new dimension is invalid.");

	dim = Size2i(46341, 46341);
	bit_map.create(dim);
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 512), "We should still have the same dimensions as before, because the new dimension is too large (46341*46341=2147488281).");

	ERR_PRINT_ON
}

TEST_CASE("[BitMap] Create bit map from image alpha") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};
	bit_map.create(dim);

	ERR_PRINT_OFF

	const Ref<Image> null_img = nullptr;
	bit_map.create_from_image_alpha(null_img);
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 256), "Bitmap should have its old values because bitmap creation from a nullptr should fail.");

	Ref<Image> empty_img;
	empty_img.instantiate();
	bit_map.create_from_image_alpha(empty_img);
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 256), "Bitmap should have its old values because bitmap creation from an empty image should fail.");

	Ref<Image> wrong_format_img = Image::create_empty(3, 3, false, Image::Format::FORMAT_DXT1);
	bit_map.create_from_image_alpha(wrong_format_img);
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 256), "Bitmap should have its old values because converting from a compressed image should fail.");

	ERR_PRINT_ON

	Ref<Image> img = Image::create_empty(3, 3, false, Image::Format::FORMAT_RGBA8);
	img->set_pixel(0, 0, Color(0, 0, 0, 0));
	img->set_pixel(0, 1, Color(0, 0, 0, 0.09f));
	img->set_pixel(0, 2, Color(0, 0, 0, 0.25f));
	img->set_pixel(1, 0, Color(0, 0, 0, 0.5f));
	img->set_pixel(1, 1, Color(0, 0, 0, 0.75f));
	img->set_pixel(1, 2, Color(0, 0, 0, 0.99f));
	img->set_pixel(2, 0, Color(0, 0, 0, 1.f));

	// Check different threshold values.
	bit_map.create_from_image_alpha(img);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 5, "There are 5 values in the image that are smaller than the default threshold of 0.1.");

	bit_map.create_from_image_alpha(img, 0.08f);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 6, "There are 6 values in the image that are smaller than the threshold of 0.08.");

	bit_map.create_from_image_alpha(img, 1);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "There are no values in the image that are smaller than the threshold of 1, there is one value equal to 1, but we check for inequality only.");
}

TEST_CASE("[BitMap] Set bit") {
	Size2i dim{ 256, 256 };
	BitMap bit_map{};

	// Setting a point before a bit map is created should not crash, because there are checks to see if we are out of bounds.
	ERR_PRINT_OFF

	bit_map.set_bitv(Point2i(128, 128), true);

	bit_map.create(dim);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "All values should be initialized to false.");
	bit_map.set_bitv(Point2i(128, 128), true);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 1, "One bit should be set to true.");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 128)) == true, "The bit at (128,128) should be set to true");

	bit_map.set_bitv(Point2i(128, 128), false);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "The bit should now be set to false again");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 128)) == false, "The bit at (128,128) should now be set to false again");

	bit_map.create(dim);
	bit_map.set_bitv(Point2i(512, 512), true);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "Nothing should change as we were trying to edit a bit outside of the correct range.");

	ERR_PRINT_ON
}

TEST_CASE("[BitMap] Get bit") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};

	ERR_PRINT_OFF

	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 128)) == false, "Trying to access a bit outside of the BitMap's range should always return false");

	bit_map.create(dim);
	CHECK(bit_map.get_bitv(Point2i(128, 128)) == false);

	bit_map.set_bit_rect(Rect2i(-1, -1, 257, 257), true);

	// Checking that range is [0, 256).
	CHECK(bit_map.get_bitv(Point2i(-1, 0)) == false);
	CHECK(bit_map.get_bitv(Point2i(0, 0)) == true);
	CHECK(bit_map.get_bitv(Point2i(128, 128)) == true);
	CHECK(bit_map.get_bitv(Point2i(255, 255)) == true);
	CHECK(bit_map.get_bitv(Point2i(256, 256)) == false);
	CHECK(bit_map.get_bitv(Point2i(257, 257)) == false);

	ERR_PRINT_ON
}

TEST_CASE("[BitMap] Set bit rect") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};

	// Although we have not setup the BitMap yet, this should not crash because we get an empty intersection inside of the method.
	bit_map.set_bit_rect(Rect2i{ 0, 0, 128, 128 }, true);

	bit_map.create(dim);
	CHECK(bit_map.get_true_bit_count() == 0);

	bit_map.set_bit_rect(Rect2i{ 0, 0, 256, 256 }, true);
	CHECK(bit_map.get_true_bit_count() == 65536);

	reset_bit_map(bit_map);

	// Checking out of bounds handling.

	ERR_PRINT_OFF

	bit_map.set_bit_rect(Rect2i{ 128, 128, 256, 256 }, true);
	CHECK(bit_map.get_true_bit_count() == 16384);

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i{ -128, -128, 256, 256 }, true);
	CHECK(bit_map.get_true_bit_count() == 16384);

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i{ -128, -128, 512, 512 }, true);
	CHECK(bit_map.get_true_bit_count() == 65536);

	ERR_PRINT_ON
}

TEST_CASE("[BitMap] Get true bit count") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};

	CHECK(bit_map.get_true_bit_count() == 0);

	bit_map.create(dim);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "Uninitialized bit map should have no true bits");
	bit_map.set_bit_rect(Rect2i{ 0, 0, 256, 256 }, true);
	CHECK(bit_map.get_true_bit_count() == 65536);
	bit_map.set_bitv(Point2i{ 0, 0 }, false);
	CHECK(bit_map.get_true_bit_count() == 65535);
	bit_map.set_bit_rect(Rect2i{ 0, 0, 256, 256 }, false);
	CHECK(bit_map.get_true_bit_count() == 0);
}

TEST_CASE("[BitMap] Get size") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};

	CHECK_MESSAGE(bit_map.get_size() == Size2i(0, 0), "Uninitialized bit map should have a size of 0x0");

	bit_map.create(dim);
	CHECK(bit_map.get_size() == Size2i(256, 256));

	ERR_PRINT_OFF
	bit_map.create(Size2i(-1, 0));
	ERR_PRINT_ON
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 256), "Invalid size should not be accepted by create");

	bit_map.create(Size2i(256, 128));
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 128), "Bitmap should have updated size");
}

TEST_CASE("[BitMap] Resize") {
	const Size2i dim{ 128, 128 };
	BitMap bit_map{};

	bit_map.resize(dim);
	CHECK(bit_map.get_size() == dim);

	bit_map.create(dim);
	bit_map.set_bit_rect(Rect2i(0, 0, 10, 10), true);
	bit_map.set_bit_rect(Rect2i(118, 118, 10, 10), true);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 200, "There should be 100 bits in the top left corner, and 100 bits in the bottom right corner");
	bit_map.resize(Size2i(64, 64));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 50, "There should be 25 bits in the top left corner, and 25 bits in the bottom right corner");

	bit_map.create(dim);
	ERR_PRINT_OFF
	bit_map.resize(Size2i(-1, 128));
	ERR_PRINT_ON
	CHECK_MESSAGE(bit_map.get_size() == Size2i(128, 128), "When an invalid size is given the bit map will keep its size");

	bit_map.create(dim);
	bit_map.set_bit_rect(Rect2i(0, 0, 10, 10), true);
	bit_map.set_bit_rect(Rect2i(118, 118, 10, 10), true);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 200, "There should be 100 bits in the top left corner, and 100 bits in the bottom right corner");
	bit_map.resize(Size2i(256, 256));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 800, "There should still be 100 bits in the bottom right corner, and all new bits should be initialized to false");
	CHECK_MESSAGE(bit_map.get_size() == Size2i(256, 256), "The bitmap should now be 256x256");
}

TEST_CASE("[BitMap] Grow and shrink mask") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};
	ERR_PRINT_OFF
	bit_map.grow_mask(100, Rect2i(0, 0, 128, 128)); // Check if method does not crash when working with an uninitialized bit map.
	ERR_PRINT_ON
	CHECK_MESSAGE(bit_map.get_size() == Size2i(0, 0), "Size should still be equal to 0x0");

	bit_map.create(dim);

	bit_map.set_bit_rect(Rect2i(96, 96, 64, 64), true);

	CHECK_MESSAGE(bit_map.get_true_bit_count() == 4096, "Creating a square of 64x64 should be 4096 bits");
	bit_map.grow_mask(0, Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 4096, "Growing with size of 0 should not change any bits");

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i(96, 96, 64, 64), true);

	CHECK_MESSAGE(bit_map.get_bitv(Point2i(95, 128)) == false, "Bits just outside of the square should not be set");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(160, 128)) == false, "Bits just outside of the square should not be set");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 95)) == false, "Bits just outside of the square should not be set");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 160)) == false, "Bits just outside of the square should not be set");
	bit_map.grow_mask(1, Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 4352, "We should have 4*64 (perimeter of square) more bits set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(95, 128)) == true, "Bits that were just outside of the square should now be set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(160, 128)) == true, "Bits that were just outside of the square should now be set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 95)) == true, "Bits that were just outside of the square should now be set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(128, 160)) == true, "Bits that were just outside of the square should now be set to true");

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i(127, 127, 1, 1), true);

	CHECK(bit_map.get_true_bit_count() == 1);
	bit_map.grow_mask(32, Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 3209, "Creates a circle around the initial bit with a radius of 32 bits. Any bit that has a distance within this radius will be set to true");

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i(127, 127, 1, 1), true);
	for (int i = 0; i < 32; i++) {
		bit_map.grow_mask(1, Rect2i(0, 0, 256, 256));
	}
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 2113, "Creates a diamond around the initial bit with diagonals that are 65 bits long.");

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i(123, 123, 10, 10), true);

	CHECK(bit_map.get_true_bit_count() == 100);
	bit_map.grow_mask(-11, Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 0, "Shrinking by more than the width of the square should totally remove it.");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(96, 96, 64, 64), true);

	CHECK_MESSAGE(bit_map.get_bitv(Point2i(96, 129)) == true, "Bits on the edge of the square should be true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(159, 129)) == true, "Bits on the edge of the square should be true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(129, 96)) == true, "Bits on the edge of the square should be true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(129, 159)) == true, "Bits on the edge of the square should be true");
	bit_map.grow_mask(-1, Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 3844, "Shrinking by 1 should set 4*63=252 bits to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(96, 129)) == false, "Bits that were on the edge of the square should now be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(159, 129)) == false, "Bits that were on the edge of the square should now be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(129, 96)) == false, "Bits that were on the edge of the square should now be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(129, 159)) == false, "Bits that were on the edge of the square should now be set to false");

	reset_bit_map(bit_map);

	bit_map.set_bit_rect(Rect2i(125, 125, 1, 6), true);
	bit_map.set_bit_rect(Rect2i(130, 125, 1, 6), true);
	bit_map.set_bit_rect(Rect2i(125, 130, 6, 1), true);

	CHECK(bit_map.get_true_bit_count() == 16);
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(125, 131)) == false, "Bits that are on the edge of the shape should be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(131, 131)) == false, "Bits that are on the edge of the shape should be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(125, 124)) == false, "Bits that are on the edge of the shape should be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(130, 124)) == false, "Bits that are on the edge of the shape should be set to false");
	bit_map.grow_mask(1, Rect2i(0, 0, 256, 256));
	CHECK(bit_map.get_true_bit_count() == 48);
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(125, 131)) == true, "Bits that were on the edge of the shape should now be set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(131, 130)) == true, "Bits that were on the edge of the shape should now be set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(125, 124)) == true, "Bits that were on the edge of the shape should now be set to true");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(130, 124)) == true, "Bits that were on the edge of the shape should now be set to true");

	CHECK_MESSAGE(bit_map.get_bitv(Point2i(124, 124)) == false, "Bits that are on the edge of the shape should be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(126, 124)) == false, "Bits that are on the edge of the shape should be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(124, 131)) == false, "Bits that are on the edge of the shape should be set to false");
	CHECK_MESSAGE(bit_map.get_bitv(Point2i(131, 131)) == false, "Bits that are on the edge of the shape should be set to false");
}

TEST_CASE("[BitMap] Blit") {
	Point2i blit_pos{ 128, 128 };
	Point2i bit_map_size{ 256, 256 };
	Point2i blit_size{ 32, 32 };

	BitMap bit_map{};
	Ref<BitMap> blit_bit_map{};

	// Testing null reference to blit bit map.
	ERR_PRINT_OFF
	bit_map.blit(blit_pos, blit_bit_map);
	ERR_PRINT_ON

	blit_bit_map.instantiate();

	// Testing if uninitialized blit bit map and uninitialized bit map does not crash
	bit_map.blit(blit_pos, blit_bit_map);

	// Testing if uninitialized bit map does not crash
	blit_bit_map->create(blit_size);
	bit_map.blit(blit_pos, blit_bit_map);

	// Testing if uninitialized bit map does not crash
	blit_bit_map.unref();
	blit_bit_map.instantiate();
	CHECK_MESSAGE(blit_bit_map->get_size() == Point2i(0, 0), "Size should be cleared by unref and instance calls.");
	bit_map.create(bit_map_size);
	bit_map.blit(Point2i(128, 128), blit_bit_map);

	// Testing if both initialized does not crash.
	blit_bit_map->create(blit_size);
	bit_map.blit(blit_pos, blit_bit_map);

	bit_map.set_bit_rect(Rect2i{ 127, 127, 3, 3 }, true);
	CHECK(bit_map.get_true_bit_count() == 9);
	bit_map.blit(Point2i(112, 112), blit_bit_map);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 9, "No bits should have been changed, as the blit bit map only contains falses");

	bit_map.create(bit_map_size);
	blit_bit_map->create(blit_size);
	blit_bit_map->set_bit_rect(Rect2i(15, 15, 3, 3), true);
	CHECK(blit_bit_map->get_true_bit_count() == 9);

	CHECK(bit_map.get_true_bit_count() == 0);
	bit_map.blit(Point2i(112, 112), blit_bit_map);
	CHECK_MESSAGE(bit_map.get_true_bit_count() == 9, "All true bits should have been moved to the bit map");
	for (int x = 127; x < 129; ++x) {
		for (int y = 127; y < 129; ++y) {
			CHECK_MESSAGE(bit_map.get_bitv(Point2i(x, y)) == true, "All true bits should have been moved to the bit map");
		}
	}
}

TEST_CASE("[BitMap] Convert to image") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};
	Ref<Image> img;

	ERR_PRINT_OFF
	img = bit_map.convert_to_image();
	ERR_PRINT_ON
	CHECK_MESSAGE(img.is_valid(), "We should receive a valid Image Object even if BitMap is not created yet");
	CHECK_MESSAGE(img->get_format() == Image::FORMAT_L8, "We should receive a valid Image Object even if BitMap is not created yet");
	CHECK_MESSAGE(img->get_size() == (Size2i(0, 0)), "Image should have no width or height, because BitMap has not yet been created");

	bit_map.create(dim);
	img = bit_map.convert_to_image();
	CHECK_MESSAGE(img->get_size() == dim, "Image should have the same dimensions as the BitMap");
	CHECK_MESSAGE(img->get_pixel(0, 0).is_equal_approx(Color(0, 0, 0)), "BitMap is initialized to all 0's, so Image should be all black");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(0, 0, 128, 128), true);
	img = bit_map.convert_to_image();
	CHECK_MESSAGE(img->get_pixel(0, 0).is_equal_approx(Color(1, 1, 1)), "BitMap's top-left quadrant is all 1's, so Image should be white");
	CHECK_MESSAGE(img->get_pixel(255, 255).is_equal_approx(Color(0, 0, 0)), "All other quadrants were 0's, so these should be black");
}

TEST_CASE("[BitMap] Clip to polygon") {
	const Size2i dim{ 256, 256 };
	BitMap bit_map{};
	Vector<Vector<Vector2>> polygons;

	ERR_PRINT_OFF
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 128, 128));
	ERR_PRINT_ON
	CHECK_MESSAGE(polygons.size() == 0, "We should have no polygons, because the BitMap was not initialized");

	bit_map.create(dim);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 128, 128));
	CHECK_MESSAGE(polygons.size() == 0, "We should have no polygons, because the BitMap was all 0's");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(0, 0, 64, 64), true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 128, 128));
	CHECK_MESSAGE(polygons.size() == 1, "We should have exactly 1 polygon");
	CHECK_MESSAGE(polygons[0].size() == 4, "The polygon should have exactly 4 points");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(0, 0, 32, 32), true);
	bit_map.set_bit_rect(Rect2i(64, 64, 32, 32), true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 128, 128));
	CHECK_MESSAGE(polygons.size() == 2, "We should have exactly 2 polygons");
	CHECK_MESSAGE(polygons[0].size() == 4, "The polygon should have exactly 4 points");
	CHECK_MESSAGE(polygons[1].size() == 4, "The polygon should have exactly 4 points");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(124, 112, 8, 32), true);
	bit_map.set_bit_rect(Rect2i(112, 124, 32, 8), true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(polygons.size() == 1, "We should have exactly 1 polygon");
	CHECK_MESSAGE(polygons[0].size() == 12, "The polygon should have exactly 12 points");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(124, 112, 8, 32), true);
	bit_map.set_bit_rect(Rect2i(112, 124, 32, 8), true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 128, 128));
	CHECK_MESSAGE(polygons.size() == 1, "We should have exactly 1 polygon");
	CHECK_MESSAGE(polygons[0].size() == 6, "The polygon should have exactly 6 points");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(0, 0, 64, 64), true);
	bit_map.set_bit_rect(Rect2i(64, 64, 64, 64), true);
	bit_map.set_bit_rect(Rect2i(192, 128, 64, 64), true);
	bit_map.set_bit_rect(Rect2i(128, 192, 64, 64), true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 256, 256));
	CHECK_MESSAGE(polygons.size() == 4, "We should have exactly 4 polygons");
	CHECK_MESSAGE(polygons[0].size() == 4, "The polygon should have exactly 4 points");
	CHECK_MESSAGE(polygons[1].size() == 4, "The polygon should have exactly 4 points");
	CHECK_MESSAGE(polygons[2].size() == 4, "The polygon should have exactly 4 points");
	CHECK_MESSAGE(polygons[3].size() == 4, "The polygon should have exactly 4 points");

	reset_bit_map(bit_map);
	bit_map.set_bit(0, 0, true);
	bit_map.set_bit(2, 0, true);
	bit_map.set_bit_rect(Rect2i(1, 1, 1, 2), true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 3, 3));
	CHECK_MESSAGE(polygons.size() == 3, "We should have exactly 3 polygons");
	CHECK_MESSAGE(polygons[0].size() == 4, "The polygon should have exactly 4 points");
	CHECK_MESSAGE(polygons[1].size() == 4, "The polygon should have exactly 4 points");
	CHECK_MESSAGE(polygons[2].size() == 4, "The polygon should have exactly 4 points");

	reset_bit_map(bit_map);
	bit_map.set_bit_rect(Rect2i(0, 0, 2, 1), true);
	bit_map.set_bit_rect(Rect2i(0, 2, 3, 1), true);
	bit_map.set_bit(0, 1, true);
	bit_map.set_bit(2, 1, true);
	polygons = bit_map.clip_opaque_to_polygons(Rect2i(0, 0, 4, 4));
	CHECK_MESSAGE(polygons.size() == 1, "We should have exactly 1 polygon");
	CHECK_MESSAGE(polygons[0].size() == 6, "The polygon should have exactly 6 points");
}

} // namespace TestBitmap
