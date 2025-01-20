# FastBlur

An Avisynth Filter: Fast Gaussian-approximate blur for all colour spaces.

(C)2019-2024 by 

```
Usage
=====

FastBlur(
  (clip)
  (float)
  (float)    y_blur
  (int)      iterations   = 3
  (bool)     dither       = false
  (bool)     gamma        = true
  (float)    threads      = -1
)


Parameters
==========

[float]:
    Blur radius (equivalent to PhotoShop Gaussian Blur's radius)

y_blur:
    Vertical blur (if different from horizontal blur)

iterations:
    Number of iterations for approximation. Defaults to 3. A value of 1 performs a box blur.

dither:
    Enable/disable dithering

gamma:
    Approximate gamma awareness. This should be disabled for masks.

threads:
     < 0 : automatic
       0 : use as many threads as logical processors
		 < 1 : use floor(logical processors*threads) threads
    >= 1 : use floor(threads) threads

```
    
# Links
Doom9 forum discussion: https://forum.doom9.org/showthread.php?t=176564

Avisynth wiki page: http://avisynth.nl/index.php/FastBlur

Author's filter collection: https://horman.net/avisynth/
