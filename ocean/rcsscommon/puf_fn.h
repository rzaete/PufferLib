#pragma once

#ifdef PUF_GPU_ENV
#define PUF_FN __host__ __device__
#else
#define PUF_FN
#endif
