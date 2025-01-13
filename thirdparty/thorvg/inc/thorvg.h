#ifndef _THORVG_H_
#define _THORVG_H_

#include <functional>
#include <memory>
#include <string>
#include <list>

#ifdef TVG_API
    #undef TVG_API
#endif

#ifndef TVG_STATIC
    #ifdef _WIN32
        #if TVG_BUILD
            #define TVG_API __declspec(dllexport)
        #else
            #define TVG_API __declspec(dllimport)
        #endif
    #elif (defined(__SUNPRO_C)  || defined(__SUNPRO_CC))
        #define TVG_API __global
    #else
        #if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__INTEL_COMPILER)
            #define TVG_API __attribute__ ((visibility("default")))
        #else
            #define TVG_API
        #endif
    #endif
#else
    #define TVG_API
#endif

#ifdef TVG_DEPRECATED
    #undef TVG_DEPRECATED
#endif

#ifdef _WIN32
    #define TVG_DEPRECATED __declspec(deprecated)
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
    #define TVG_DEPRECATED __attribute__ ((__deprecated__))
#else
    #define TVG_DEPRECATED
#endif

#define _TVG_DECLARE_PRIVATE(A) \
    struct Impl; \
    Impl* pImpl; \
protected: \
    A(const A&) = delete; \
    const A& operator=(const A&) = delete; \
    A()

#define _TVG_DISABLE_CTOR(A) \
    A() = delete; \
    ~A() = delete

#define _TVG_DECLARE_ACCESSOR(A) \
    friend A

namespace tvg
{

class RenderMethod;
class Animation;

/**
 * @defgroup ThorVG ThorVG
 * @brief ThorVG classes and enumerations providing C++ APIs.
 */

/**@{*/

/**
 * @brief Enumeration specifying the result from the APIs.
 *
 * All ThorVG APIs could potentially return one of the values in the list.
 * Please note that some APIs may additionally specify the reasons that trigger their return values.
 *
 */
enum class Result
{
    Success = 0,           ///< The value returned in case of a correct request execution.
    InvalidArguments,      ///< The value returned in the event of a problem with the arguments given to the API - e.g. empty paths or null pointers.
    InsufficientCondition, ///< The value returned in case the request cannot be processed - e.g. asking for properties of an object, which does not exist.
    FailedAllocation,      ///< The value returned in case of unsuccessful memory allocation.
    MemoryCorruption,      ///< The value returned in the event of bad memory handling - e.g. failing in pointer releasing or casting
    NonSupport,            ///< The value returned in case of choosing unsupported engine features(options).
    Unknown                ///< The value returned in all other cases.
};


/**
 * @brief Enumeration specifying the values of the path commands accepted by TVG.
 *
 * Not to be confused with the path commands from the svg path element (like M, L, Q, H and many others).
 * TVG interprets all of them and translates to the ones from the PathCommand values.
 */
enum class PathCommand
{
    Close = 0, ///< Ends the current sub-path and connects it with its initial point. This command doesn't expect any points.
    MoveTo,    ///< Sets a new initial point of the sub-path and a new current point. This command expects 1 point: the starting position.
    LineTo,    ///< Draws a line from the current point to the given point and sets a new value of the current point. This command expects 1 point: the end-position of the line.
    CubicTo    ///< Draws a cubic Bezier curve from the current point to the given point using two given control points and sets a new value of the current point. This command expects 3 points: the 1st control-point, the 2nd control-point, the end-point of the curve.
};


/**
 * @brief Enumeration determining the ending type of a stroke in the open sub-paths.
 */
enum class StrokeCap
{
    Square = 0, ///< The stroke is extended in both end-points of a sub-path by a rectangle, with the width equal to the stroke width and the length equal to the half of the stroke width. For zero length sub-paths the square is rendered with the size of the stroke width.
    Round,      ///< The stroke is extended in both end-points of a sub-path by a half circle, with a radius equal to the half of a stroke width. For zero length sub-paths a full circle is rendered.
    Butt        ///< The stroke ends exactly at each of the two end-points of a sub-path. For zero length sub-paths no stroke is rendered.
};


/**
 * @brief Enumeration determining the style used at the corners of joined stroked path segments.
 */
enum class StrokeJoin
{
    Bevel = 0, ///< The outer corner of the joined path segments is bevelled at the join point. The triangular region of the corner is enclosed by a straight line between the outer corners of each stroke.
    Round,     ///< The outer corner of the joined path segments is rounded. The circular region is centered at the join point.
    Miter      ///< The outer corner of the joined path segments is spiked. The spike is created by extension beyond the join point of the outer edges of the stroke until they intersect. In case the extension goes beyond the limit, the join style is converted to the Bevel style.
};


/**
 * @brief Enumeration specifying how to fill the area outside the gradient bounds.
 */
enum class FillSpread
{
    Pad = 0, ///< The remaining area is filled with the closest stop color.
    Reflect, ///< The gradient pattern is reflected outside the gradient area until the expected region is filled.
    Repeat   ///< The gradient pattern is repeated continuously beyond the gradient area until the expected region is filled.
};


/**
 * @brief Enumeration specifying the algorithm used to establish which parts of the shape are treated as the inside of the shape.
 */
enum class FillRule
{
    Winding = 0, ///< A line from the point to a location outside the shape is drawn. The intersections of the line with the path segment of the shape are counted. Starting from zero, if the path segment of the shape crosses the line clockwise, one is added, otherwise one is subtracted. If the resulting sum is non zero, the point is inside the shape.
    EvenOdd      ///< A line from the point to a location outside the shape is drawn and its intersections with the path segments of the shape are counted. If the number of intersections is an odd number, the point is inside the shape.
};


/**
 * @brief Enumeration indicating the method used in the composition of two objects - the target and the source.
 *
 * Notation: S(Source), T(Target), SA(Source Alpha), TA(Target Alpha)
 *
 * @see Paint::composite()
 */
enum class CompositeMethod
{
    None = 0,           ///< No composition is applied.
    ClipPath,           ///< The intersection of the source and the target is determined and only the resulting pixels from the source are rendered. Note that ClipPath only supports the Shape type. @deprecated Use Paint::clip() instead.
    AlphaMask,          ///< Alpha Masking using the compositing target's pixels as an alpha value.
    InvAlphaMask,       ///< Alpha Masking using the complement to the compositing target's pixels as an alpha value.
    LumaMask,           ///< Alpha Masking using the grayscale (0.2125R + 0.7154G + 0.0721*B) of the compositing target's pixels. @since 0.9
    InvLumaMask,        ///< Alpha Masking using the grayscale (0.2125R + 0.7154G + 0.0721*B) of the complement to the compositing target's pixels. @since 0.11
    AddMask,            ///< Combines the target and source objects pixels using target alpha. (T * TA) + (S * (255 - TA)) (Experimental API)
    SubtractMask,       ///< Subtracts the source color from the target color while considering their respective target alpha. (T * TA) - (S * (255 - TA)) (Experimental API)
    IntersectMask,      ///< Computes the result by taking the minimum value between the target alpha and the source alpha and multiplies it with the target color. (T * min(TA, SA)) (Experimental API)
    DifferenceMask,     ///< Calculates the absolute difference between the target color and the source color multiplied by the complement of the target alpha. abs(T - S * (255 - TA)) (Experimental API)
    LightenMask,        ///< Where multiple masks intersect, the highest transparency value is used. (Experimental API)
    DarkenMask          ///< Where multiple masks intersect, the lowest transparency value is used. (Experimental API)
};


/**
 * @brief Enumeration indicates the method used for blending paint. Please refer to the respective formulas for each method.
 *
 * Notation: S(source paint as the top layer), D(destination as the bottom layer), Sa(source paint alpha), Da(destination alpha)
 *
 * @see Paint::blend()
 *
 * @since 0.15
 */
enum class BlendMethod : uint8_t
{
    Normal = 0,        ///< Perform the alpha blending(default). S if (Sa == 255), otherwise (Sa * S) + (255 - Sa) * D
    Multiply,          ///< Takes the RGB channel values from 0 to 255 of each pixel in the top layer and multiples them with the values for the corresponding pixel from the bottom layer. (S * D)
    Screen,            ///< The values of the pixels in the two layers are inverted, multiplied, and then inverted again. (S + D) - (S * D)
    Overlay,           ///< Combines Multiply and Screen blend modes. (2 * S * D) if (2 * D < Da), otherwise (Sa * Da) - 2 * (Da - S) * (Sa - D)
    Darken,            ///< Creates a pixel that retains the smallest components of the top and bottom layer pixels. min(S, D)
    Lighten,           ///< Only has the opposite action of Darken Only. max(S, D)
    ColorDodge,        ///< Divides the bottom layer by the inverted top layer. D / (255 - S)
    ColorBurn,         ///< Divides the inverted bottom layer by the top layer, and then inverts the result. 255 - (255 - D) / S
    HardLight,         ///< The same as Overlay but with the color roles reversed. (2 * S * D) if (S < Sa), otherwise (Sa * Da) - 2 * (Da - S) * (Sa - D)
    SoftLight,         ///< The same as Overlay but with applying pure black or white does not result in pure black or white. (1 - 2 * S) * (D ^ 2) + (2 * S * D)
    Difference,        ///< Subtracts the bottom layer from the top layer or the other way around, to always get a non-negative value. (S - D) if (S > D), otherwise (D - S)
    Exclusion,         ///< The result is twice the product of the top and bottom layers, subtracted from their sum. s + d - (2 * s * d)
    Hue,               ///< Reserved. Not supported.
    Saturation,        ///< Reserved. Not supported.
    Color,             ///< Reserved. Not supported.
    Luminosity,        ///< Reserved. Not supported.
    Add,               ///< Simply adds pixel values of one layer with the other. (S + D)
    HardMix            ///< Reserved. Not supported.
};


/**
 * @brief Enumeration that defines methods used for Scene Effects.
 *
 * This enum provides options to apply various post-processing effects to a scene.
 * Scene effects are typically applied to modify the final appearance of a rendered scene, such as blurring.
 *
 * @see Scene::push(SceneEffect effect, ...)
 *
 * @note Experimental API
 */
enum class SceneEffect : uint8_t
{
    ClearAll = 0,      ///< Reset all previously applied scene effects, restoring the scene to its original state.
    GaussianBlur,      ///< Apply a blur effect with a Gaussian filter. Param(3) = {sigma(float)[> 0], direction(int)[both: 0 / horizontal: 1 / vertical: 2], border(int)[duplicate: 0 / wrap: 1], quality(int)[0 - 100]}
    DropShadow         ///< Apply a drop shadow effect with a Gaussian Blur filter. Param(8) = {color_R(int)[0 - 255], color_G(int)[0 - 255], color_B(int)[0 - 255], opacity(int)[0 - 255], angle(float)[0 - 360], distance(float), blur_sigma(float)[> 0], quality(int)[0 - 100]}
};


/**
 * @brief Enumeration specifying the engine type used for the graphics backend. For multiple backends bitwise operation is allowed.
 */
enum class CanvasEngine
{
    Sw = (1 << 1), ///< CPU rasterizer.
    Gl = (1 << 2), ///< OpenGL rasterizer.
    Wg = (1 << 3), ///< WebGPU rasterizer. @since 0.15
};


/**
 * @brief Enumeration specifying the ThorVG class type value.
 *
 * ThorVG's drawing objects can return class type values, allowing you to identify the specific class of each object.
 *
 * @see Paint::type()
 * @see Fill::type()
 *
 * @note Experimental API
 */
enum class Type : uint8_t
{
    Undefined = 0,         ///< Unkown class
    Shape,                 ///< Shape class
    Scene,                 ///< Scene class
    Picture,               ///< Picture class
    Text,                  ///< Text class
    LinearGradient = 10,   ///< LinearGradient class
    RadialGradient         ///< RadialGradient class
};


/**
 * @brief A data structure representing a point in two-dimensional space.
 */
struct Point
{
    float x, y;
};


/**
 * @brief A data structure representing a three-dimensional matrix.
 *
 * The elements e11, e12, e21 and e22 represent the rotation matrix, including the scaling factor.
 * The elements e13 and e23 determine the translation of the object along the x and y-axis, respectively.
 * The elements e31 and e32 are set to 0, e33 is set to 1.
 */
struct Matrix
{
    float e11, e12, e13;
    float e21, e22, e23;
    float e31, e32, e33;
};


/**
 * @class Paint
 *
 * @brief An abstract class for managing graphical elements.
 *
 * A graphical element in TVG is any object composed into a Canvas.
 * Paint represents such a graphical object and its behaviors such as duplication, transformation and composition.
 * TVG recommends the user to regard a paint as a set of volatile commands. They can prepare a Paint and then request a Canvas to run them.
 */
class TVG_API Paint
{
public:
    virtual ~Paint();

    /**
     * @brief Sets the angle by which the object is rotated.
     *
     * The angle in measured clockwise from the horizontal axis.
     * The rotational axis passes through the point on the object with zero coordinates.
     *
     * @param[in] degree The value of the angle in degrees.
     *
     * @retval Result::InsufficientCondition in case a custom transform is applied.
     * @see Paint::transform()
     */
    Result rotate(float degree) noexcept;

    /**
     * @brief Sets the scale value of the object.
     *
     * @param[in] factor The value of the scaling factor. The default value is 1.
     *
     * @retval Result::InsufficientCondition in case a custom transform is applied.
     * @see Paint::transform()
     */
    Result scale(float factor) noexcept;

    /**
     * @brief Sets the values by which the object is moved in a two-dimensional space.
     *
     * The origin of the coordinate system is in the upper-left corner of the canvas.
     * The horizontal and vertical axes point to the right and down, respectively.
     *
     * @param[in] x The value of the horizontal shift.
     * @param[in] y The value of the vertical shift.
     *
     * @retval Result::InsufficientCondition in case a custom transform is applied.
     * @see Paint::transform()
     */
    Result translate(float x, float y) noexcept;

    /**
     * @brief Sets the matrix of the affine transformation for the object.
     *
     * The augmented matrix of the transformation is expected to be given.
     *
     * @param[in] m The 3x3 augmented matrix.
     */
    Result transform(const Matrix& m) noexcept;

    /**
     * @brief Gets the matrix of the affine transformation of the object.
     *
     * The values of the matrix can be set by the transform() API, as well by the translate(),
     * scale() and rotate(). In case no transformation was applied, the identity matrix is returned.
     *
     * @return The augmented transformation matrix.
     *
     * @since 0.4
     */
    Matrix transform() noexcept;

    /**
     * @brief Sets the opacity of the object.
     *
     * @param[in] o The opacity value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     *
     * @note Setting the opacity with this API may require multiple render pass for composition. It is recommended to avoid changing the opacity if possible.
     */
    Result opacity(uint8_t o) noexcept;

    /**
     * @brief Sets the composition target object and the composition method.
     *
     * @param[in] target The paint of the target object.
     * @param[in] method The method used to composite the source object with the target.
     */
    Result composite(std::unique_ptr<Paint> target, CompositeMethod method) noexcept;

    /**
     * @brief Clip the drawing region of the paint object.
     *
     * This function restricts the drawing area of the paint object to the specified shape's paths.
     *
     * @param[in] clipper The shape object as the clipper.
     *
     * @retval Result::NonSupport If the @p clipper type is not Shape.
     *
     * @note @p clipper only supports the Shape type.
     * @note Experimental API
     */
    Result clip(std::unique_ptr<Paint> clipper) noexcept;

    /**
     * @brief Sets the blending method for the paint object.
     *
     * The blending feature allows you to combine colors to create visually appealing effects, including transparency, lighting, shading, and color mixing, among others.
     * its process involves the combination of colors or images from the source paint object with the destination (the lower layer image) using blending operations.
     * The blending operation is determined by the chosen @p BlendMethod, which specifies how the colors or images are combined.
     *
     * @param[in] method The blending method to be set.
     *
     * @note Experimental API
     */
    Result blend(BlendMethod method) noexcept;

    /**
     * @deprecated Use bounds(float* x, float* y, float* w, float* h, bool transformed) instead
     */
    TVG_DEPRECATED Result bounds(float* x, float* y, float* w, float* h) const noexcept;

    /**
     * @brief Gets the axis-aligned bounding box of the paint object.
     *
     * @param[out] x The x-coordinate of the upper-left corner of the object.
     * @param[out] y The y-coordinate of the upper-left corner of the object.
     * @param[out] w The width of the object.
     * @param[out] h The height of the object.
     * @param[in] transformed If @c true, the paint's transformations are taken into account in the scene it belongs to. Otherwise they aren't.
     *
     * @note This is useful when you need to figure out the bounding box of the paint in the canvas space.
     * @note The bounding box doesn't indicate the actual drawing region. It's the smallest rectangle that encloses the object.
     * @note If @p transformed is @c true, the paint needs to be pushed into a canvas and updated before this api is called.
     * @see Canvas::update()
     */
    Result bounds(float* x, float* y, float* w, float* h, bool transformed) const noexcept;

    /**
     * @brief Duplicates the object.
     *
     * Creates a new object and sets its all properties as in the original object.
     *
     * @return The created object when succeed, @c nullptr otherwise.
     */
    Paint* duplicate() const noexcept;

    /**
     * @brief Gets the opacity value of the object.
     *
     * @return The opacity value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     */
    uint8_t opacity() const noexcept;

    /**
     * @brief Gets the composition target object and the composition method.
     *
     * @param[out] target The paint of the target object.
     *
     * @return The method used to composite the source object with the target.
     *
     * @since 0.5
     */
    CompositeMethod composite(const Paint** target) const noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the Paint instance.
     *
     * @since Experimental API
     */
    virtual Type type() const noexcept = 0;

    /**
     * @brief Unique ID of this instance.
     *
     * This is reserved to specify an paint instance in a scene.
     *
     * @since Experimental API
     */
    uint32_t id = 0;

    /**
     * @see Paint::type()
     */
    TVG_DEPRECATED uint32_t identifier() const noexcept;

    _TVG_DECLARE_PRIVATE(Paint);
};


/**
 * @class Fill
 *
 * @brief An abstract class representing the gradient fill of the Shape object.
 *
 * It contains the information about the gradient colors and their arrangement
 * inside the gradient bounds. The gradients bounds are defined in the LinearGradient
 * or RadialGradient class, depending on the type of the gradient to be used.
 * It specifies the gradient behavior in case the area defined by the gradient bounds
 * is smaller than the area to be filled.
 */
class TVG_API Fill
{
public:
    /**
     * @brief A data structure storing the information about the color and its relative position inside the gradient bounds.
     */
    struct ColorStop
    {
        float offset; /**< The relative position of the color. */
        uint8_t r;    /**< The red color channel value in the range [0 ~ 255]. */
        uint8_t g;    /**< The green color channel value in the range [0 ~ 255]. */
        uint8_t b;    /**< The blue color channel value in the range [0 ~ 255]. */
        uint8_t a;    /**< The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque. */
    };

    virtual ~Fill();

    /**
     * @brief Sets the parameters of the colors of the gradient and their position.
     *
     * @param[in] colorStops An array of ColorStop data structure.
     * @param[in] cnt The count of the @p colorStops array equal to the colors number used in the gradient.
     */
    Result colorStops(const ColorStop* colorStops, uint32_t cnt) noexcept;

    /**
     * @brief Sets the FillSpread value, which specifies how to fill the area outside the gradient bounds.
     *
     * @param[in] s The FillSpread value.
     */
    Result spread(FillSpread s) noexcept;

    /**
     * @brief Sets the matrix of the affine transformation for the gradient fill.
     *
     * The augmented matrix of the transformation is expected to be given.
     *
     * @param[in] m The 3x3 augmented matrix.
     */
    Result transform(const Matrix& m) noexcept;

    /**
     * @brief Gets the parameters of the colors of the gradient, their position and number.
     *
     * @param[out] colorStops A pointer to the memory location, where the array of the gradient's ColorStop is stored.
     *
     * @return The number of colors used in the gradient. This value corresponds to the length of the @p colorStops array.
     */
    uint32_t colorStops(const ColorStop** colorStops) const noexcept;

    /**
     * @brief Gets the FillSpread value of the fill.
     *
     * @return The FillSpread value of this Fill.
     */
    FillSpread spread() const noexcept;

    /**
     * @brief Gets the matrix of the affine transformation of the gradient fill.
     *
     * In case no transformation was applied, the identity matrix is returned.
     *
     * @return The augmented transformation matrix.
     */
    Matrix transform() const noexcept;

    /**
     * @brief Creates a copy of the Fill object.
     *
     * Return a newly created Fill object with the properties copied from the original.
     *
     * @return A copied Fill object when succeed, @c nullptr otherwise.
     */
    Fill* duplicate() const noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the Fill instance.
     *
     * @since Experimental API
     */
    virtual Type type() const noexcept = 0;

    /**
     * @see Fill::type()
     */
    TVG_DEPRECATED uint32_t identifier() const noexcept;

    _TVG_DECLARE_PRIVATE(Fill);
};


/**
 * @class Canvas
 *
 * @brief An abstract class for drawing graphical elements.
 *
 * A canvas is an entity responsible for drawing the target. It sets up the drawing engine and the buffer, which can be drawn on the screen. It also manages given Paint objects.
 *
 * @note A Canvas behavior depends on the raster engine though the final content of the buffer is expected to be identical.
 * @warning The Paint objects belonging to one Canvas can't be shared among multiple Canvases.
 */
class TVG_API Canvas
{
public:
    Canvas(RenderMethod*);
    virtual ~Canvas();

    TVG_DEPRECATED Result reserve(uint32_t n) noexcept;

    /**
     * @brief Returns the list of the paints that currently held by the Canvas.
     *
     * This function provides the list of paint nodes, allowing users a direct opportunity to modify the scene tree.
     *
     * @warning Please avoid accessing the paints during Canvas update/draw. You can access them after calling sync().
     * @see Canvas::sync()
     *
     * @note Experimental API
     */
    std::list<Paint*>& paints() noexcept;

    /**
     * @brief Passes drawing elements to the Canvas using Paint objects.
     *
     * Only pushed paints in the canvas will be drawing targets.
     * They are retained by the canvas until you call Canvas::clear().
     *
     * @param[in] paint A Paint object to be drawn.
     *
     * @retval Result::MemoryCorruption In case a @c nullptr is passed as the argument.
     *
     * @note The rendering order of the paints is the same as the order as they were pushed into the canvas. Consider sorting the paints before pushing them if you intend to use layering.
     * @see Canvas::paints()
     * @see Canvas::clear()
     */
    virtual Result push(std::unique_ptr<Paint> paint) noexcept;

    /**
     * @brief Clear the internal canvas resources that used for the drawing.
     *
     * This API sets the total number of paints pushed into the canvas to zero.
     * Depending on the value of the @p free argument, the paints are either freed or retained.
     * So if you need to update paint properties while maintaining the existing scene structure, you can set @p free = false.
     *
     * @param[in] free If @c true, the memory occupied by paints is deallocated, otherwise it is not.
     *
     *
     * @see Canvas::push()
     * @see Canvas::paints()
     */
    virtual Result clear(bool free = true) noexcept;

    /**
     * @brief Request the canvas to update the paint objects.
     *
     * If a @c nullptr is passed all paint objects retained by the Canvas are updated,
     * otherwise only the paint to which the given @p paint points.
     *
     * @param[in] paint A pointer to the Paint object or @c nullptr.
     *
     * @note The Update behavior can be asynchronous if the assigned thread number is greater than zero.
     */
    virtual Result update(Paint* paint = nullptr) noexcept;

    /**
     * @brief Requests the canvas to draw the Paint objects.
     *
     * @note Drawing can be asynchronous if the assigned thread number is greater than zero. To guarantee the drawing is done, call sync() afterwards.
     * @see Canvas::sync()
     */
    virtual Result draw() noexcept;

    /**
     * @brief Sets the drawing region in the canvas.
     *
     * This function defines the rectangular area of the canvas that will be used for drawing operations.
     * The specified viewport is used to clip the rendering output to the boundaries of the rectangle.
     *
     * @param[in] x The x-coordinate of the upper-left corner of the rectangle.
     * @param[in] y The y-coordinate of the upper-left corner of the rectangle.
     * @param[in] w The width of the rectangle.
     * @param[in] h The height of the rectangle.
     *
     * @see SwCanvas::target()
     * @see GlCanvas::target()
     * @see WgCanvas::target()
     *
     * @warning It's not allowed to change the viewport during Canvas::push() - Canvas::sync() or Canvas::update() - Canvas::sync().
     *
     * @note When resetting the target, the viewport will also be reset to the target size.
     * @since 0.15
     */
    virtual Result viewport(int32_t x, int32_t y, int32_t w, int32_t h) noexcept;

    /**
     * @brief Guarantees that drawing task is finished.
     *
     * The Canvas rendering can be performed asynchronously. To make sure that rendering is finished,
     * the sync() must be called after the draw() regardless of threading.
     *
     * @retval Result::InsufficientCondition: The canvas is either already in sync condition or in a damaged condition (a draw is required before syncing).
     *
     * @see Canvas::draw()
     */
    virtual Result sync() noexcept;

    _TVG_DECLARE_PRIVATE(Canvas);
};


/**
 * @class LinearGradient
 *
 * @brief A class representing the linear gradient fill of the Shape object.
 *
 * Besides the APIs inherited from the Fill class, it enables setting and getting the linear gradient bounds.
 * The behavior outside the gradient bounds depends on the value specified in the spread API.
 */
class TVG_API LinearGradient final : public Fill
{
public:
    ~LinearGradient();

    /**
     * @brief Sets the linear gradient bounds.
     *
     * The bounds of the linear gradient are defined as a surface constrained by two parallel lines crossing
     * the given points (@p x1, @p y1) and (@p x2, @p y2), respectively. Both lines are perpendicular to the line linking
     * (@p x1, @p y1) and (@p x2, @p y2).
     *
     * @param[in] x1 The horizontal coordinate of the first point used to determine the gradient bounds.
     * @param[in] y1 The vertical coordinate of the first point used to determine the gradient bounds.
     * @param[in] x2 The horizontal coordinate of the second point used to determine the gradient bounds.
     * @param[in] y2 The vertical coordinate of the second point used to determine the gradient bounds.
     *
     * @note In case the first and the second points are equal, an object is filled with a single color using the last color specified in the colorStops().
     * @see Fill::colorStops()
     */
    Result linear(float x1, float y1, float x2, float y2) noexcept;

    /**
     * @brief Gets the linear gradient bounds.
     *
     * The bounds of the linear gradient are defined as a surface constrained by two parallel lines crossing
     * the given points (@p x1, @p y1) and (@p x2, @p y2), respectively. Both lines are perpendicular to the line linking
     * (@p x1, @p y1) and (@p x2, @p y2).
     *
     * @param[out] x1 The horizontal coordinate of the first point used to determine the gradient bounds.
     * @param[out] y1 The vertical coordinate of the first point used to determine the gradient bounds.
     * @param[out] x2 The horizontal coordinate of the second point used to determine the gradient bounds.
     * @param[out] y2 The vertical coordinate of the second point used to determine the gradient bounds.
     */
    Result linear(float* x1, float* y1, float* x2, float* y2) const noexcept;

    /**
     * @brief Creates a new LinearGradient object.
     *
     * @return A new LinearGradient object.
     */
    static std::unique_ptr<LinearGradient> gen() noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the LinearGradient instance.
     *
     * @since Experimental API
     */
    Type type() const noexcept override;

    /**
     * @see LinearGradient::type()
     */
    TVG_DEPRECATED static uint32_t identifier() noexcept;

    _TVG_DECLARE_PRIVATE(LinearGradient);
};


/**
 * @class RadialGradient
 *
 * @brief A class representing the radial gradient fill of the Shape object.
 *
 */
class TVG_API RadialGradient final : public Fill
{
public:
    ~RadialGradient();

    /**
     * @brief Sets the radial gradient bounds.
     *
     * The radial gradient bounds are defined as a circle centered in a given point (@p cx, @p cy) of a given radius.
     *
     * @param[in] cx The horizontal coordinate of the center of the bounding circle.
     * @param[in] cy The vertical coordinate of the center of the bounding circle.
     * @param[in] radius The radius of the bounding circle.
     *
     * @retval Result::InvalidArguments in case the @p radius value is zero or less.
     *
     * @note In case the @p radius is zero, an object is filled with a single color using the last color specified in the colorStops().
     */
    Result radial(float cx, float cy, float radius) noexcept;

    /**
     * @brief Gets the radial gradient bounds.
     *
     * The radial gradient bounds are defined as a circle centered in a given point (@p cx, @p cy) of a given radius.
     *
     * @param[out] cx The horizontal coordinate of the center of the bounding circle.
     * @param[out] cy The vertical coordinate of the center of the bounding circle.
     * @param[out] radius The radius of the bounding circle.
     *
     */
    Result radial(float* cx, float* cy, float* radius) const noexcept;

    /**
     * @brief Creates a new RadialGradient object.
     *
     * @return A new RadialGradient object.
     */
    static std::unique_ptr<RadialGradient> gen() noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the LinearGradient instance.
     *
     * @since Experimental API
     */
    Type type() const noexcept override;

    /**
     * @see RadialGradient::type()
     */
    TVG_DEPRECATED static uint32_t identifier() noexcept;

    _TVG_DECLARE_PRIVATE(RadialGradient);
};


/**
 * @class Shape
 *
 * @brief A class representing two-dimensional figures and their properties.
 *
 * A shape has three major properties: shape outline, stroking, filling. The outline in the Shape is retained as the path.
 * Path can be composed by accumulating primitive commands such as moveTo(), lineTo(), cubicTo(), or complete shape interfaces such as appendRect(), appendCircle(), etc.
 * Path can consists of sub-paths. One sub-path is determined by a close command.
 *
 * The stroke of Shape is an optional property in case the Shape needs to be represented with/without the outline borders.
 * It's efficient since the shape path and the stroking path can be shared with each other. It's also convenient when controlling both in one context.
 */
class TVG_API Shape final : public Paint
{
public:
    ~Shape();

    /**
     * @brief Resets the shape path.
     *
     * The transformation matrix, color, fill, and stroke properties are retained.
     *
     * @note The memory where the path data is stored is not deallocated at this stage to allow for caching.
     */
    Result reset() noexcept;

    /**
     * @brief Sets the initial point of the sub-path.
     *
     * The value of the current point is set to the given point.
     *
     * @param[in] x The horizontal coordinate of the initial point of the sub-path.
     * @param[in] y The vertical coordinate of the initial point of the sub-path.
     */
    Result moveTo(float x, float y) noexcept;

    /**
     * @brief Adds a new point to the sub-path, which results in drawing a line from the current point to the given end-point.
     *
     * The value of the current point is set to the given end-point.
     *
     * @param[in] x The horizontal coordinate of the end-point of the line.
     * @param[in] y The vertical coordinate of the end-point of the line.
     *
     * @note In case this is the first command in the path, it corresponds to the moveTo() call.
     */
    Result lineTo(float x, float y) noexcept;

    /**
     * @brief Adds new points to the sub-path, which results in drawing a cubic Bezier curve starting
     * at the current point and ending at the given end-point (@p x, @p y) using the control points (@p cx1, @p cy1) and (@p cx2, @p cy2).
     *
     * The value of the current point is set to the given end-point.
     *
     * @param[in] cx1 The horizontal coordinate of the 1st control point.
     * @param[in] cy1 The vertical coordinate of the 1st control point.
     * @param[in] cx2 The horizontal coordinate of the 2nd control point.
     * @param[in] cy2 The vertical coordinate of the 2nd control point.
     * @param[in] x The horizontal coordinate of the end-point of the curve.
     * @param[in] y The vertical coordinate of the end-point of the curve.
     *
     * @note In case this is the first command in the path, no data from the path are rendered.
     */
    Result cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y) noexcept;

    /**
     * @brief Closes the current sub-path by drawing a line from the current point to the initial point of the sub-path.
     *
     * The value of the current point is set to the initial point of the closed sub-path.
     *
     * @note In case the sub-path does not contain any points, this function has no effect.
     */
    Result close() noexcept;

    /**
     * @brief Appends a rectangle to the path.
     *
     * The rectangle with rounded corners can be achieved by setting non-zero values to @p rx and @p ry arguments.
     * The @p rx and @p ry values specify the radii of the ellipse defining the rounding of the corners.
     *
     * The position of the rectangle is specified by the coordinates of its upper-left corner - @p x and @p y arguments.
     *
     * The rectangle is treated as a new sub-path - it is not connected with the previous sub-path.
     *
     * The value of the current point is set to (@p x + @p rx, @p y) - in case @p rx is greater
     * than @p w/2 the current point is set to (@p x + @p w/2, @p y)
     *
     * @param[in] x The horizontal coordinate of the upper-left corner of the rectangle.
     * @param[in] y The vertical coordinate of the upper-left corner of the rectangle.
     * @param[in] w The width of the rectangle.
     * @param[in] h The height of the rectangle.
     * @param[in] rx The x-axis radius of the ellipse defining the rounded corners of the rectangle.
     * @param[in] ry The y-axis radius of the ellipse defining the rounded corners of the rectangle.
     *
     * @note For @p rx and @p ry greater than or equal to the half of @p w and the half of @p h, respectively, the shape become an ellipse.
     */
    Result appendRect(float x, float y, float w, float h, float rx = 0, float ry = 0) noexcept;

    /**
     * @brief Appends an ellipse to the path.
     *
     * The position of the ellipse is specified by the coordinates of its center - @p cx and @p cy arguments.
     *
     * The ellipse is treated as a new sub-path - it is not connected with the previous sub-path.
     *
     * The value of the current point is set to (@p cx, @p cy - @p ry).
     *
     * @param[in] cx The horizontal coordinate of the center of the ellipse.
     * @param[in] cy The vertical coordinate of the center of the ellipse.
     * @param[in] rx The x-axis radius of the ellipse.
     * @param[in] ry The y-axis radius of the ellipse.
     *
     */
    Result appendCircle(float cx, float cy, float rx, float ry) noexcept;

    /**
     * @brief Appends a circular arc to the path.
     *
     * The arc is treated as a new sub-path - it is not connected with the previous sub-path.
     * The current point value is set to the end-point of the arc in case @p pie is @c false, and to the center of the arc otherwise.
     *
     * @param[in] cx The horizontal coordinate of the center of the arc.
     * @param[in] cy The vertical coordinate of the center of the arc.
     * @param[in] radius The radius of the arc.
     * @param[in] startAngle The start angle of the arc given in degrees, measured counter-clockwise from the horizontal line.
     * @param[in] sweep The central angle of the arc given in degrees, measured counter-clockwise from @p startAngle.
     * @param[in] pie Specifies whether to draw radii from the arc's center to both of its end-point - drawn if @c true.
     *
     * @note Setting @p sweep value greater than 360 degrees, is equivalent to calling appendCircle(cx, cy, radius, radius).
     */
    TVG_DEPRECATED Result appendArc(float cx, float cy, float radius, float startAngle, float sweep, bool pie) noexcept;

    /**
     * @brief Appends a given sub-path to the path.
     *
     * The current point value is set to the last point from the sub-path.
     * For each command from the @p cmds array, an appropriate number of points in @p pts array should be specified.
     * If the number of points in the @p pts array is different than the number required by the @p cmds array, the shape with this sub-path will not be displayed on the screen.
     *
     * @param[in] cmds The array of the commands in the sub-path.
     * @param[in] cmdCnt The number of the sub-path's commands.
     * @param[in] pts The array of the two-dimensional points.
     * @param[in] ptsCnt The number of the points in the @p pts array.
     *
     * @note The interface is designed for optimal path setting if the caller has a completed path commands already.
     */
    Result appendPath(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept;

    /**
     * @brief Sets the stroke width for all of the figures from the path.
     *
     * @param[in] width The width of the stroke. The default value is 0.
     *
     */
    Result stroke(float width) noexcept;

    /**
     * @brief Sets the color of the stroke for all of the figures from the path.
     *
     * @param[in] r The red color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] g The green color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] b The blue color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque. The default value is 0.
     *
     */
    Result stroke(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept;

    /**
     * @brief Sets the gradient fill of the stroke for all of the figures from the path.
     *
     * @param[in] f The gradient fill.
     *
     * @retval Result::MemoryCorruption In case a @c nullptr is passed as the argument.
     */
    Result stroke(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Sets the dash pattern of the stroke.
     *
     * @param[in] dashPattern The array of consecutive pair values of the dash length and the gap length.
     * @param[in] cnt The length of the @p dashPattern array.
     *
     * @retval Result::InvalidArguments In case @p dashPattern is @c nullptr and @p cnt > 0, @p cnt is zero, any of the dash pattern values is zero or less.
     *
     * @note To reset the stroke dash pattern, pass @c nullptr to @p dashPattern and zero to @p cnt.
     * @warning @p cnt must be greater than 1 if the dash pattern is valid.
     */
    Result stroke(const float* dashPattern, uint32_t cnt) noexcept;

    /**
     * @brief Sets the cap style of the stroke in the open sub-paths.
     *
     * @param[in] cap The cap style value. The default value is @c StrokeCap::Square.
     *
     */
    Result stroke(StrokeCap cap) noexcept;

    /**
     * @brief Sets the join style for stroked path segments.
     *
     * The join style is used for joining the two line segment while stroking the path.
     *
     * @param[in] join The join style value. The default value is @c StrokeJoin::Bevel.
     *
     */
    Result stroke(StrokeJoin join) noexcept;

    /**
     * @brief Sets the stroke miterlimit.
     *
     * @param[in] miterlimit The miterlimit imposes a limit on the extent of the stroke join, when the @c StrokeJoin::Miter join style is set. The default value is 4.
     *
     * @retval Result::InvalidArgument for @p miterlimit values less than zero.
     * 
     * @since 0.11
     */
    Result strokeMiterlimit(float miterlimit) noexcept;

    /**
     * @brief Sets the trim of the stroke along the defined path segment, allowing control over which part of the stroke is visible.
     *
     * If the values of the arguments @p begin and @p end exceed the 0-1 range, they are wrapped around in a manner similar to angle wrapping, effectively treating the range as circular.
     *
     * @param[in] begin Specifies the start of the segment to display along the path.
     * @param[in] end Specifies the end of the segment to display along the path.
     * @param[in] simultaneous Determines how to trim multiple paths within a single shape. If set to @c true (default), trimming is applied simultaneously to all paths;
     * Otherwise, all paths are treated as a single entity with a combined length equal to the sum of their individual lengths and are trimmed as such.
     *
     * @note Experimental API
     */
    Result strokeTrim(float begin, float end, bool simultaneous = true) noexcept;

    /**
     * @brief Sets the solid color for all of the figures from the path.
     *
     * The parts of the shape defined as inner are colored.
     *
     * @param[in] r The red color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] g The green color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] b The blue color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque. The default value is 0.
     *
     * @note Either a solid color or a gradient fill is applied, depending on what was set as last.
     */
    Result fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept;

    /**
     * @brief Sets the gradient fill for all of the figures from the path.
     *
     * The parts of the shape defined as inner are filled.
     *
     * @param[in] f The unique pointer to the gradient fill.
     *
     * @note Either a solid color or a gradient fill is applied, depending on what was set as last.
     */
    Result fill(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Sets the fill rule for the Shape object.
     *
     * @param[in] r The fill rule value. The default value is @c FillRule::Winding.
     */
    Result fill(FillRule r) noexcept;

    /**
     * @brief Sets the rendering order of the stroke and the fill.
     *
     * @param[in] strokeFirst If @c true the stroke is rendered before the fill, otherwise the stroke is rendered as the second one (the default option).
     *
     * @since 0.10
     */
    Result order(bool strokeFirst) noexcept;

    /**
     * @brief Gets the commands data of the path.
     *
     * @param[out] cmds The pointer to the array of the commands from the path.
     *
     * @return The length of the @p cmds array when succeed, zero otherwise.
     */
    uint32_t pathCommands(const PathCommand** cmds) const noexcept;

    /**
     * @brief Gets the points values of the path.
     *
     * @param[out] pts The pointer to the array of the two-dimensional points from the path.
     *
     * @return The length of the @p pts array when succeed, zero otherwise.
     */
    uint32_t pathCoords(const Point** pts) const noexcept;

    /**
     * @brief Gets the pointer to the gradient fill of the shape.
     *
     * @return The pointer to the gradient fill of the stroke when succeed, @c nullptr in case no fill was set.
     */
    const Fill* fill() const noexcept;

    /**
     * @brief Gets the solid color of the shape.
     *
     * @param[out] r The red color channel value in the range [0 ~ 255].
     * @param[out] g The green color channel value in the range [0 ~ 255].
     * @param[out] b The blue color channel value in the range [0 ~ 255].
     * @param[out] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     *
     */
    Result fillColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a = nullptr) const noexcept;

    /**
     * @brief Gets the fill rule value.
     *
     * @return The fill rule value of the shape.
     */
    FillRule fillRule() const noexcept;

    /**
     * @brief Gets the stroke width.
     *
     * @return The stroke width value when succeed, zero if no stroke was set.
     */
    float strokeWidth() const noexcept;

    /**
     * @brief Gets the color of the shape's stroke.
     *
     * @param[out] r The red color channel value in the range [0 ~ 255].
     * @param[out] g The green color channel value in the range [0 ~ 255].
     * @param[out] b The blue color channel value in the range [0 ~ 255].
     * @param[out] a The alpha channel value in the range [0 ~ 255], where 0 is completely transparent and 255 is opaque.
     *
     */
    Result strokeColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a = nullptr) const noexcept;

    /**
     * @brief Gets the pointer to the gradient fill of the stroke.
     *
     * @return The pointer to the gradient fill of the stroke when succeed, @c nullptr otherwise.
     */
    const Fill* strokeFill() const noexcept;

    /**
     * @brief Gets the dash pattern of the stroke.
     *
     * @param[out] dashPattern The pointer to the memory, where the dash pattern array is stored.
     *
     * @return The length of the @p dashPattern array.
     */
    uint32_t strokeDash(const float** dashPattern) const noexcept;

    /**
     * @brief Gets the cap style used for stroking the path.
     *
     * @return The cap style value of the stroke.
     */
    StrokeCap strokeCap() const noexcept;

    /**
     * @brief Gets the join style value used for stroking the path.
     *
     * @return The join style value of the stroke.
     */
    StrokeJoin strokeJoin() const noexcept;

    /**
     * @brief Gets the stroke miterlimit.
     *
     * @return The stroke miterlimit value when succeed, 4 if no stroke was set.
     *
     * @since 0.11
     */
    float strokeMiterlimit() const noexcept;

    /**
     * @brief Creates a new Shape object.
     *
     * @return A new Shape object.
     */
    static std::unique_ptr<Shape> gen() noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the Shape instance.
     *
     * @since Experimental API
     */
    Type type() const noexcept override;

    /**
     * @see Shape::type()
     */
    TVG_DEPRECATED static uint32_t identifier() noexcept;

    _TVG_DECLARE_PRIVATE(Shape);
};


/**
 * @class Picture
 *
 * @brief A class representing an image read in one of the supported formats: raw, svg, png, jpg, lottie(json) and etc.
 * Besides the methods inherited from the Paint, it provides methods to load & draw images on the canvas.
 *
 * @note Supported formats are depended on the available TVG loaders.
 * @note See Animation class if the picture data is animatable.
 */
class TVG_API Picture final : public Paint
{
public:
    ~Picture();

    /**
     * @brief Loads a picture data directly from a file.
     *
     * ThorVG efficiently caches the loaded data using the specified @p path as a key.
     * This means that loading the same file again will not result in duplicate operations;
     * instead, ThorVG will reuse the previously loaded picture data.
     *
     * @param[in] path A path to the picture file.
     *
     * @retval Result::InvalidArguments In case the @p path is invalid.
     * @retval Result::NonSupport When trying to load a file with an unknown extension.
     *
     * @note The Load behavior can be asynchronous if the assigned thread number is greater than zero.
     * @see Initializer::init()
     */
    Result load(const std::string& path) noexcept;

    /**
     * @deprecated Use load(const char* data, uint32_t size, const std::string& mimeType, bool copy) instead.
     */
    TVG_DEPRECATED Result load(const char* data, uint32_t size, bool copy = false) noexcept;

    /**
     * @brief Loads a picture data from a memory block of a given size.
     *
     * ThorVG efficiently caches the loaded data using the specified @p data address as a key
     * when the @p copy has @c false. This means that loading the same data again will not result in duplicate operations
     * for the sharable @p data. Instead, ThorVG will reuse the previously loaded picture data.
     *
     * @param[in] data A pointer to a memory location where the content of the picture file is stored. A null-terminated string is expected for non-binary data if @p copy is @c false.
     * @param[in] size The size in bytes of the memory occupied by the @p data.
     * @param[in] mimeType Mimetype or extension of data such as "jpg", "jpeg", "lottie", "svg", "svg+xml", "png", etc. In case an empty string or an unknown type is provided, the loaders will be tried one by one.
     * @param[in] copy If @c true the data are copied into the engine local buffer, otherwise they are not.
     *
     * @retval Result::InvalidArguments In case no data are provided or the @p size is zero or less.
     * @retval Result::NonSupport When trying to load a file with an unknown extension.
     *
     * @warning It's the user responsibility to release the @p data memory.
     *
     * @note If you are unsure about the MIME type, you can provide an empty value like @c "", and thorvg will attempt to figure it out.
     * @since 0.5
     */
    Result load(const char* data, uint32_t size, const std::string& mimeType, bool copy = false) noexcept;

    /**
     * @brief Resizes the picture content to the given width and height.
     *
     * The picture content is resized while keeping the default size aspect ratio.
     * The scaling factor is established for each of dimensions and the smaller value is applied to both of them.
     *
     * @param[in] w A new width of the image in pixels.
     * @param[in] h A new height of the image in pixels.
     *
     */
    Result size(float w, float h) noexcept;

    /**
     * @brief Gets the size of the image.
     *
     * @param[out] w The width of the image in pixels.
     * @param[out] h The height of the image in pixels.
     *
     */
    Result size(float* w, float* h) const noexcept;

    /**
     * @brief Loads raw data in ARGB8888 format from a memory block of the given size.
     *
     * ThorVG efficiently caches the loaded data using the specified @p data address as a key
     * when the @p copy has @c false. This means that loading the same data again will not result in duplicate operations
     * for the sharable @p data. Instead, ThorVG will reuse the previously loaded picture data.
     *
     * @param[in] data A pointer to a memory location where the content of the picture raw data is stored.
     * @param[in] w The width of the image @p data in pixels.
     * @param[in] h The height of the image @p data in pixels.
     * @param[in] premultiplied If @c true, the given image data is alpha-premultiplied.
     * @param[in] copy If @c true the data are copied into the engine local buffer, otherwise they are not.
     *
     * @since 0.9
     */
    Result load(uint32_t* data, uint32_t w, uint32_t h, bool copy) noexcept;

    /**
     * @brief Retrieve a paint object from the Picture scene by its Unique ID.
     *
     * This function searches for a paint object within the Picture scene that matches the provided @p id.
     *
     * @param[in] id The Unique ID of the paint object.
     *
     * @return A pointer to the paint object that matches the given identifier, or @c nullptr if no matching paint object is found.
     *
     * @see Accessor::id()
     *
     * @note Experimental API
     */
    const Paint* paint(uint32_t id) noexcept;

    /**
     * @brief Creates a new Picture object.
     *
     * @return A new Picture object.
     */
    static std::unique_ptr<Picture> gen() noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the Picture instance.
     *
     * @since Experimental API
     */
    Type type() const noexcept override;

    /**
     * @see Picture::type()
     */
    TVG_DEPRECATED static uint32_t identifier() noexcept;

    _TVG_DECLARE_ACCESSOR(Animation);
    _TVG_DECLARE_PRIVATE(Picture);
};


/**
 * @class Scene
 *
 * @brief A class to composite children paints.
 *
 * As the traditional graphics rendering method, TVG also enables scene-graph mechanism.
 * This feature supports an array function for managing the multiple paints as one group paint.
 *
 * As a group, the scene can be transformed, made translucent and composited with other target paints,
 * its children will be affected by the scene world.
 */
class TVG_API Scene final : public Paint
{
public:
    ~Scene();

    /**
     * @brief Passes drawing elements to the Scene using Paint objects.
     *
     * Only the paints pushed into the scene will be the drawn targets.
     * The paints are retained by the scene until Scene::clear() is called.
     *
     * @param[in] paint A Paint object to be drawn.
     *
     * @note The rendering order of the paints is the same as the order as they were pushed. Consider sorting the paints before pushing them if you intend to use layering.
     * @see Scene::paints()
     * @see Scene::clear()
     */
    Result push(std::unique_ptr<Paint> paint) noexcept;

    TVG_DEPRECATED Result reserve(uint32_t size) noexcept;

    /**
     * @brief Returns the list of the paints that currently held by the Scene.
     *
     * This function provides the list of paint nodes, allowing users a direct opportunity to modify the scene tree.
     *
     * @warning Please avoid accessing the paints during Scene update/draw. You can access them after calling Canvas::sync().
     * @see Canvas::sync()
     * @see Scene::push(std::unique_ptr<Paint> paint)
     * @see Scene::clear()
     *
     * @note Experimental API
     */
    std::list<Paint*>& paints() noexcept;

    /**
     * @brief Sets the total number of the paints pushed into the scene to be zero.
     * Depending on the value of the @p free argument, the paints are freed or not.
     *
     * @param[in] free If @c true, the memory occupied by paints is deallocated, otherwise it is not.
     *
     * @warning If you don't free the paints they become dangled. They are supposed to be reused, otherwise you are responsible for their lives. Thus please use the @p free argument only when you know how it works, otherwise it's not recommended.
     *
     * @since 0.2
     */
    Result clear(bool free = true) noexcept;

    /**
     * @brief Apply a post-processing effect to the scene.
     *
     * This function adds a specified scene effect, such as clearing all effects or applying a Gaussian blur,
     * to the scene after it has been rendered. Multiple effects can be applied in sequence.
     *
     * @param[in] effect The scene effect to apply. Options are defined in the SceneEffect enum.
     *                   For example, use SceneEffect::GaussianBlur to apply a blur with specific parameters.
     * @param[in] ... Additional variadic parameters required for certain effects (e.g., sigma and direction for GaussianBlur).
     *
     * @note Experimental API
     */
    Result push(SceneEffect effect, ...) noexcept;

    /**
     * @brief Creates a new Scene object.
     *
     * @return A new Scene object.
     */
    static std::unique_ptr<Scene> gen() noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the Scene instance.
     *
     * @since Experimental API
     */
    Type type() const noexcept override;

    /**
     * @see Scene::type()
     */
    TVG_DEPRECATED static uint32_t identifier() noexcept;

    _TVG_DECLARE_PRIVATE(Scene);
};


/**
 * @class Text
 *
 * @brief A class to represent text objects in a graphical context, allowing for rendering and manipulation of unicode text.
 *
 * @since 0.15
 */
class TVG_API Text final : public Paint
{
public:
    ~Text();

    /**
     * @brief Sets the font properties for the text.
     *
     * This function allows you to define the font characteristics used for text rendering.
     * It sets the font name, size and optionally the style.
     *
     * @param[in] name The name of the font. This should correspond to a font available in the canvas.
     * @param[in] size The size of the font in points. This determines how large the text will appear.
     * @param[in] style The style of the font. It can be used to set the font to 'italic'.
     *                  If not specified, the default style is used. Only 'italic' style is supported currently.
     *
     * @retval Result::InsufficientCondition when the specified @p name cannot be found.
     *
     * @note Experimental API
     */
    Result font(const char* name, float size, const char* style = nullptr) noexcept;

    /**
     * @brief Assigns the given unicode text to be rendered.
     *
     * This function sets the unicode string that will be displayed by the rendering system.
     * The text is set according to the specified UTF encoding method, which defaults to UTF-8.
     *
     * @param[in] text The multi-byte text encoded with utf8 string to be rendered.
     *
     * @note Experimental API
     */
    Result text(const char* text) noexcept;

    /**
     * @brief Sets the text color.
     *
     * @param[in] r The red color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] g The green color channel value in the range [0 ~ 255]. The default value is 0.
     * @param[in] b The blue color channel value in the range [0 ~ 255]. The default value is 0.
     *
     * @see Text::font()
     *
     * @since 0.15
     */
    Result fill(uint8_t r, uint8_t g, uint8_t b) noexcept;

    /**
     * @brief Sets the gradient fill for all of the figures from the text.
     *
     * The parts of the text defined as inner are filled.
     *
     * @param[in] f The unique pointer to the gradient fill.
     *
     * @note Either a solid color or a gradient fill is applied, depending on what was set as last.
     * @see Text::font()
     *
     * @since 0.15
     */
    Result fill(std::unique_ptr<Fill> f) noexcept;

    /**
     * @brief Loads a scalable font data (ttf) from a file.
     *
     * ThorVG efficiently caches the loaded data using the specified @p path as a key.
     * This means that loading the same file again will not result in duplicate operations;
     * instead, ThorVG will reuse the previously loaded font data.
     *
     * @param[in] path The path to the font file.
     *
     * @retval Result::InvalidArguments In case the @p path is invalid.
     * @retval Result::NonSupport When trying to load a file with an unknown extension.
     *
     * @see Text::unload(const std::string& path)
     *
     * @since 0.15
     */
    static Result load(const std::string& path) noexcept;

    /**
     * @brief Loads a scalable font data (ttf) from a memory block of a given size.
     *
     * ThorVG efficiently caches the loaded font data using the specified @p name as a key.
     * This means that loading the same fonts again will not result in duplicate operations.
     * Instead, ThorVG will reuse the previously loaded font data.
     *
     * @param[in] name The name under which the font will be stored and accessible (e.x. in a @p font() API).
     * @param[in] data A pointer to a memory location where the content of the font data is stored.
     * @param[in] size The size in bytes of the memory occupied by the @p data.
     * @param[in] mimeType Mimetype or extension of font data. In case an empty string is provided the loader will be determined automatically.
     * @param[in] copy If @c true the data are copied into the engine local buffer, otherwise they are not (default).
     *
     * @retval Result::InvalidArguments If no name is provided or if @p size is zero while @p data points to a valid memory location.
     * @retval Result::NonSupport When trying to load a file with an unsupported extension.
     * @retval Result::InsufficientCondition If attempting to unload the font data that has not been previously loaded.
     *
     * @warning It's the user responsibility to release the @p data memory.
     *
     * @note To unload the font data loaded using this API, pass the proper @p name and @c nullptr as @p data.
     * @note If you are unsure about the MIME type, you can provide an empty value like @c "", and thorvg will attempt to figure it out.
     * @see Text::font(const char* name, float size, const char* style)
     *
     * @note 0.15
     */
    static Result load(const char* name, const char* data, uint32_t size, const std::string& mimeType = "ttf", bool copy = false) noexcept;

    /**
     * @brief Unloads the specified scalable font data (TTF) that was previously loaded.
     *
     * This function is used to release resources associated with a font file that has been loaded into memory.
     *
     * @param[in] path The file path of the loaded font.
     *
     * @retval Result::InsufficientCondition Fails if the loader is not initialized.
     *
     * @note If the font data is currently in use, it will not be immediately unloaded.
     * @see Text::load(const std::string& path)
     * 
     * @since 0.15
     */
    static Result unload(const std::string& path) noexcept;

    /**
     * @brief Creates a new Text object.
     *
     * @return A new Text object.
     *
     * @since 0.15
     */
    static std::unique_ptr<Text> gen() noexcept;

    /**
     * @brief Returns the ID value of this class.
     *
     * This method can be used to check the current concrete instance type.
     *
     * @return The class type ID of the Text instance.
     *
     * @since Experimental API
     */
    Type type() const noexcept override;

    _TVG_DECLARE_PRIVATE(Text);
};


/**
 * @class SwCanvas
 *
 * @brief A class for the rendering graphical elements with a software raster engine.
 */
class TVG_API SwCanvas final : public Canvas
{
public:
    ~SwCanvas();

    /**
     * @brief Enumeration specifying the methods of combining the 8-bit color channels into 32-bit color.
     */
    enum Colorspace
    {
        ABGR8888 = 0,      ///< The channels are joined in the order: alpha, blue, green, red. Colors are alpha-premultiplied. (a << 24 | b << 16 | g << 8 | r)
        ARGB8888,          ///< The channels are joined in the order: alpha, red, green, blue. Colors are alpha-premultiplied. (a << 24 | r << 16 | g << 8 | b)
        ABGR8888S,         ///< The channels are joined in the order: alpha, blue, green, red. Colors are un-alpha-premultiplied. @since 0.12
        ARGB8888S,         ///< The channels are joined in the order: alpha, red, green, blue. Colors are un-alpha-premultiplied. @since 0.12
    };

    /**
     * @brief Enumeration specifying the methods of Memory Pool behavior policy.
     * @since 0.4
     */
    enum MempoolPolicy
    {
        Default = 0, ///< Default behavior that ThorVG is designed to.
        Shareable,   ///< Memory Pool is shared among the SwCanvases.
        Individual   ///< Allocate designated memory pool that is only used by current instance.
    };

    /**
     * @brief Sets the drawing target for the rasterization.
     *
     * The buffer of a desirable size should be allocated and owned by the caller.
     *
     * @param[in] buffer A pointer to a memory block of the size @p stride x @p h, where the raster data are stored.
     * @param[in] stride The stride of the raster image - greater than or equal to @p w.
     * @param[in] w The width of the raster image.
     * @param[in] h The height of the raster image.
     * @param[in] cs The value specifying the way the 32-bits colors should be read/written.
     *
     * @retval Result::InvalidArguments In case no valid pointer is provided or the width, or the height or the stride is zero.
     * @retval Result::InsufficientCondition if the canvas is performing rendering. Please ensure the canvas is synced.
     * @retval Result::NonSupport In case the software engine is not supported.
     *
     * @warning Do not access @p buffer during Canvas::push() - Canvas::sync(). It should not be accessed while the engine is writing on it.
     *
     * @see Canvas::viewport()
     * @see Canvas::sync()
    */
    Result target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, Colorspace cs) noexcept;

    /**
     * @brief Set sw engine memory pool behavior policy.
     *
     * Basically ThorVG draws a lot of shapes, it allocates/deallocates a few chunk of memory
     * while processing rendering. It internally uses one shared memory pool
     * which can be reused among the canvases in order to avoid memory overhead.
     *
     * Thus ThorVG suggests using a memory pool policy to satisfy user demands,
     * if it needs to guarantee the thread-safety of the internal data access.
     *
     * @param[in] policy The method specifying the Memory Pool behavior. The default value is @c MempoolPolicy::Default.
     *
     * @retval Result::InsufficientCondition If the canvas contains some paints already.
     * @retval Result::NonSupport In case the software engine is not supported.
     *
     * @note When @c policy is set as @c MempoolPolicy::Individual, the current instance of canvas uses its own individual
     *       memory data, which is not shared with others. This is necessary when the canvas is accessed on a worker-thread.
     *
     * @warning It's not allowed after pushing any paints.
     *
     * @since 0.4
    */
    Result mempool(MempoolPolicy policy) noexcept;

    /**
     * @brief Creates a new SwCanvas object.
     * @return A new SwCanvas object.
     */
    static std::unique_ptr<SwCanvas> gen() noexcept;

    _TVG_DECLARE_PRIVATE(SwCanvas);
};


/**
 * @class GlCanvas
 *
 * @brief A class for the rendering graphic elements with a GL raster engine.
 *
 * @since 0.14
 */
class TVG_API GlCanvas final : public Canvas
{
public:
    ~GlCanvas();

    /**
     * @brief Sets the drawing target for rasterization.
     *
     * This function specifies the drawing target where the rasterization will occur. It can target
     * a specific framebuffer object (FBO) or the main surface.
     *
     * @param[in] id The GL target ID, usually indicating the FBO ID. A value of @c 0 specifies the main surface.
     * @param[in] w The width (in pixels) of the raster image.
     * @param[in] h The height (in pixels) of the raster image.
     *
     * @retval Result::InsufficientCondition if the canvas is performing rendering. Please ensure the canvas is synced.
     * @retval Result::NonSupport In case the gl engine is not supported.
     *
     * @see Canvas::viewport()
     * @see Canvas::sync()
     *
     * @note Currently, this only allows the GL_RGBA8 color space format.
     * @note Experimental API
    */
    Result target(int32_t id, uint32_t w, uint32_t h) noexcept;

    /**
     * @brief Creates a new GlCanvas object.
     *
     * @return A new GlCanvas object.
     *
     * @since 0.14
     */
    static std::unique_ptr<GlCanvas> gen() noexcept;

    _TVG_DECLARE_PRIVATE(GlCanvas);
};


/**
 * @class WgCanvas
 *
 * @brief A class for the rendering graphic elements with a WebGPU raster engine.
 *
 * @warning Please do not use it. This class is not fully supported yet.
 *
 * @since 0.15
 */
class TVG_API WgCanvas final : public Canvas
{
public:
    ~WgCanvas();

    /**
     * @brief Sets the drawing target for the rasterization.
     *
     * @param[in] instance WGPUInstance, context for all other wgpu objects.
     * @param[in] surface WGPUSurface, handle to a presentable surface.
     * @param[in] w The width of the surface.
     * @param[in] h The height of the surface.
     * @param[in] device WGPUDevice, a desired handle for the wgpu device. If it is @c nullptr, ThorVG will assign an appropriate device internally.
     *
     * @retval Result::InsufficientCondition if the canvas is performing rendering. Please ensure the canvas is synced.
     * @retval Result::NonSupport In case the wg engine is not supported.
     *
     * @note Experimental API
     *
     * @see Canvas::viewport()
     * @see Canvas::sync()
     */
    Result target(void* instance, void* surface, uint32_t w, uint32_t h, void* device = nullptr) noexcept;

    /**
     * @brief Creates a new WgCanvas object.
     *
     * @return A new WgCanvas object.
     *
     * @since 0.15
     */
    static std::unique_ptr<WgCanvas> gen() noexcept;

    _TVG_DECLARE_PRIVATE(WgCanvas);
};


/**
 * @class Initializer
 *
 * @brief A class that enables initialization and termination of the TVG engines.
 */
class TVG_API Initializer final
{
public:
    /**
     * @brief Initializes TVG engines.
     *
     * TVG requires the running-engine environment.
     * TVG runs its own task-scheduler for parallelizing rendering tasks efficiently.
     * You can indicate the number of threads, the count of which is designated @p threads.
     * In the initialization step, TVG will generate/spawn the threads as set by @p threads count.
     *
     * @param[in] engine The engine types to initialize. This is relative to the Canvas types, in which it will be used. For multiple backends bitwise operation is allowed.
     * @param[in] threads The number of additional threads. Zero indicates only the main thread is to be used.
     *
     * @retval Result::NonSupport In case the engine type is not supported on the system.
     *
     * @note The Initializer keeps track of the number of times it was called. Threads count is fixed at the first init() call.
     * @see Initializer::term()
     */
    static Result init(CanvasEngine engine, uint32_t threads) noexcept;

    /**
     * @brief Terminates TVG engines.
     *
     * @param[in] engine The engine types to terminate. This is relative to the Canvas types, in which it will be used. For multiple backends bitwise operation is allowed
     *
     * @retval Result::InsufficientCondition In case there is nothing to be terminated.
     * @retval Result::NonSupport In case the engine type is not supported on the system.
     *
     * @note Initializer does own reference counting for multiple calls.
     * @see Initializer::init()
     */
    static Result term(CanvasEngine engine) noexcept;

    /**
     * @brief Retrieves the version of the TVG engine.
     *
     * @param[out] major A major version number.
     * @param[out] minor A minor version number.
     * @param[out] micro A micro version number.
     *
     * @return The version of the engine in the format major.minor.micro, or a @p nullptr in case of an internal error.
     *
     * @since 0.15
     */
    static const char* version(uint32_t* major, uint32_t* minor, uint32_t* micro) noexcept;

    _TVG_DISABLE_CTOR(Initializer);
};


/**
 * @class Animation
 *
 * @brief The Animation class enables manipulation of animatable images.
 *
 * This class supports the display and control of animation frames.
 *
 * @since 0.13
 */

class TVG_API Animation
{
public:
    ~Animation();

    /**
     * @brief Specifies the current frame in the animation.
     *
     * @param[in] no The index of the animation frame to be displayed. The index should be less than the totalFrame().
     *
     * @retval Result::InsufficientCondition if the given @p no is the same as the current frame value.
     * @retval Result::NonSupport The current Picture data does not support animations.
     *
     * @note For efficiency, ThorVG ignores updates to the new frame value if the difference from the current frame value
     *       is less than 0.001. In such cases, it returns @c Result::InsufficientCondition.
     *       Values less than 0.001 may be disregarded and may not be accurately retained by the Animation.
     *
     * @see totalFrame()
     *
     */
    Result frame(float no) noexcept;

    /**
     * @brief Retrieves a picture instance associated with this animation instance.
     *
     * This function provides access to the picture instance that can be used to load animation formats, such as Lottie(json).
     * After setting up the picture, it can be pushed to the designated canvas, enabling control over animation frames
     * with this Animation instance.
     *
     * @return A picture instance that is tied to this animation.
     *
     * @warning The picture instance is owned by Animation. It should not be deleted manually.
     *
     */
    Picture* picture() const noexcept;

    /**
     * @brief Retrieves the current frame number of the animation.
     *
     * @return The current frame number of the animation, between 0 and totalFrame() - 1.
     *
     * @note If the Picture is not properly configured, this function will return 0.
     *
     * @see Animation::frame(float no)
     * @see Animation::totalFrame()
     *
     */
    float curFrame() const noexcept;

    /**
     * @brief Retrieves the total number of frames in the animation.
     *
     * @return The total number of frames in the animation.
     *
     * @note Frame numbering starts from 0.
     * @note If the Picture is not properly configured, this function will return 0.
     *
     */
    float totalFrame() const noexcept;

    /**
     * @brief Retrieves the duration of the animation in seconds.
     *
     * @return The duration of the animation in seconds.
     *
     * @note If the Picture is not properly configured, this function will return 0.
     *
     */
    float duration() const noexcept;

    /**
     * @brief Specifies the playback segment of the animation.
     *
     * The set segment is designated as the play area of the animation.
     * This is useful for playing a specific segment within the entire animation.
     * After setting, the number of animation frames and the playback time are calculated
     * by mapping the playback segment as the entire range.
     *
     * @param[in] begin segment start.
     * @param[in] end segment end.
     *
     * @retval Result::InsufficientCondition In case the animation is not loaded.
     * @retval Result::NonSupport When it's not animatable.
     *
     * @note Animation allows a range from 0.0 to 1.0. @p end should not be higher than @p begin.
     * @note If a marker has been specified, its range will be disregarded.
     * @see LottieAnimation::segment(const char* marker)
     *
     * @note Experimental API
     */
    Result segment(float begin, float end) noexcept;

    /**
     * @brief Gets the current segment.
     *
     * @param[out] begin segment start.
     * @param[out] end segment end.
     *
     * @retval Result::InsufficientCondition In case the animation is not loaded.
     * @retval Result::NonSupport When it's not animatable.
     *
     * @note Experimental API
     */
    Result segment(float* begin, float* end = nullptr) noexcept;

    /**
     * @brief Creates a new Animation object.
     *
     * @return A new Animation object.
     *
     */
    static std::unique_ptr<Animation> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Animation);
};


/**
 * @class Saver
 *
 * @brief A class for exporting a paint object into a specified file, from which to recover the paint data later.
 *
 * ThorVG provides a feature for exporting & importing paint data. The Saver role is to export the paint data to a file.
 * It's useful when you need to save the composed scene or image from a paint object and recreate it later.
 *
 * The file format is decided by the extension name(i.e. "*.tvg") while the supported formats depend on the TVG packaging environment.
 * If it doesn't support the file format, the save() method returns the @c Result::NonSupport result.
 *
 * Once you export a paint to the file successfully, you can recreate it using the Picture class.
 *
 * @see Picture::load()
 *
 * @since 0.5
 */
class TVG_API Saver final
{
public:
    ~Saver();

    /**
     * @brief Sets the base background content for the saved image.
     *
     * @param[in] paint The paint to be drawn as the background image for the saving paint.
     *
     * @note Experimental API
     */
    Result background(std::unique_ptr<Paint> paint) noexcept;

    /**
     * @brief Exports the given @p paint data to the given @p path
     *
     * If the saver module supports any compression mechanism, it will optimize the data size.
     * This might affect the encoding/decoding time in some cases. You can turn off the compression
     * if you wish to optimize for speed.
     *
     * @param[in] paint The paint to be saved with all its associated properties.
     * @param[in] path A path to the file, in which the paint data is to be saved.
     * @param[in] compress If @c true then compress data if possible.
     *
     * @retval Result::InsufficientCondition If currently saving other resources.
     * @retval Result::NonSupport When trying to save a file with an unknown extension or in an unsupported format.
     * @retval Result::Unknown In case an empty paint is to be saved.
     *
     * @note Saving can be asynchronous if the assigned thread number is greater than zero. To guarantee the saving is done, call sync() afterwards.
     * @see Saver::sync()
     *
     * @since 0.5
     */
    Result save(std::unique_ptr<Paint> paint, const std::string& path, bool compress = true) noexcept;

    /**
     * @brief Export the provided animation data to the specified file path.
     *
     * This function exports the given animation data to the provided file path. You can also specify the desired frame rate in frames per second (FPS) by providing the fps parameter.
     *
     * @param[in] animation The animation to be saved, including all associated properties.
     * @param[in] path The path to the file where the animation will be saved.
     * @param[in] quality The encoded quality level. @c 0 is the minimum, @c 100 is the maximum value(recommended).
     * @param[in] fps The desired frames per second (FPS). For example, to encode data at 60 FPS, pass 60. Pass 0 to keep the original frame data.
     *
     * @retval Result::InsufficientCondition if there are ongoing resource-saving operations.
     * @retval Result::NonSupport if an attempt is made to save the file with an unknown extension or in an unsupported format.
     * @retval Result::Unknown if attempting to save an empty paint.
     *
     * @note A higher frames per second (FPS) would result in a larger file size. It is recommended to use the default value.
     * @note Saving can be asynchronous if the assigned thread number is greater than zero. To guarantee the saving is done, call sync() afterwards.
     *
     * @see Saver::sync()
     *
     * @note Experimental API
     */
    Result save(std::unique_ptr<Animation> animation, const std::string& path, uint32_t quality = 100, uint32_t fps = 0) noexcept;

    /**
     * @brief Guarantees that the saving task is finished.
     *
     * The behavior of the Saver works on a sync/async basis, depending on the threading setting of the Initializer.
     * Thus, if you wish to have a benefit of it, you must call sync() after the save() in the proper delayed time.
     * Otherwise, you can call sync() immediately.
     *
     * @note The asynchronous tasking is dependent on the Saver module implementation.
     * @see Saver::save()
     *
     * @since 0.5
     */
    Result sync() noexcept;

    /**
     * @brief Creates a new Saver object.
     *
     * @return A new Saver object.
     *
     * @since 0.5
     */
    static std::unique_ptr<Saver> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Saver);
};


/**
 * @class Accessor
 *
 * @brief The Accessor is a utility class to debug the Scene structure by traversing the scene-tree.
 *
 * The Accessor helps you search specific nodes to read the property information, figure out the structure of the scene tree and its size.
 *
 * @warning We strongly warn you not to change the paints of a scene unless you really know the design-structure.
 *
 * @since 0.10
 */
class TVG_API Accessor final
{
public:
    ~Accessor();

    TVG_DEPRECATED std::unique_ptr<Picture> set(std::unique_ptr<Picture> picture, std::function<bool(const Paint* paint)> func) noexcept;

    /**
     * @brief Set the access function for traversing the Picture scene tree nodes.
     *
     * @param[in] picture The picture node to traverse the internal scene-tree.
     * @param[in] func The callback function calling for every paint nodes of the Picture.
     * @param[in] data Data passed to the @p func as its argument.
     *
     * @note The bitmap based picture might not have the scene-tree.
     *
     * @note Experimental API
     */
    Result set(const Picture* picture, std::function<bool(const Paint* paint, void* data)> func, void* data) noexcept;

    /**
     * @brief Generate a unique ID (hash key) from a given name.
     *
     * This function computes a unique identifier value based on the provided string.
     * You can use this to assign a unique ID to the Paint object.
     *
     * @param[in] name The input string to generate the unique identifier from.
     *
     * @return The generated unique identifier value.
     *
     * @see Paint::id
     *
     * @note Experimental API
     */
    static uint32_t id(const char* name) noexcept;

    /**
     * @brief Creates a new Accessor object.
     *
     * @return A new Accessor object.
     */
    static std::unique_ptr<Accessor> gen() noexcept;

    _TVG_DECLARE_PRIVATE(Accessor);
};


/**
 * @brief The cast() function is a utility function used to cast a 'Paint' to type 'T'.
 * @since 0.11
 */
template<typename T = tvg::Paint>
std::unique_ptr<T> cast(Paint* paint)
{
    return std::unique_ptr<T>(static_cast<T*>(paint));
}


/**
 * @brief The cast() function is a utility function used to cast a 'Fill' to type 'T'.
 * @since 0.11
 */
template<typename T = tvg::Fill>
std::unique_ptr<T> cast(Fill* fill)
{
    return std::unique_ptr<T>(static_cast<T*>(fill));
}


/** @}*/

} //namespace

#endif //_THORVG_H_
