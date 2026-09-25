module billeeScm {
    @ Last commanded value per PCA9685 servo channel. Positional channels hold degrees (-90..+90),
    @ continuous-rotation channels hold speed (-1.0..+1.0). 0 until first commanded and after a release.
    array ServoAngles = [12] F32
}
