#define OGRE_imageLoad2D(inImage, iuv) imageLoad(inImage, int2(iuv))
#define OGRE_imageLoad2DArray(inImage, iuvw) imageLoad(inImage, int3(iuvw))

#define OGRE_imageWrite2D1(outImage, iuv, value) imageStore(outImage, int2(iuv), float4(value, 0, 0, 0))
#define OGRE_imageWrite2D2(outImage, iuv, value) imageStore(outImage, int2(iuv), float4(value, 0, 0))
#define OGRE_imageWrite2D4(outImage, iuv, value) imageStore(outImage, int2(iuv), value)

#define OGRE_imageLoad3D(inImage, iuv) imageLoad(inImage, int3(iuv))

#define OGRE_imageWrite3D1(outImage, iuv, value) imageStore(outImage, int3(iuv), value)
#define OGRE_imageWrite3D4(outImage, iuv, value) imageStore(outImage, int3(iuv), value)

#define OGRE_imageWrite2DArray1(outImage, iuvw, value) imageStore(outImage, int3(iuvw), value)
#define OGRE_imageWrite2DArray4(outImage, iuvw, value) imageStore(outImage, int3(iuvw), value)

//#define sharedOnlyBarrier memoryBarrierShared();barrier();
