#ifndef CAPACITIVE_MOISTURE_H
#define CAPACITIVE_MOISTURE_H

#define CALIBRATEMIN 0 // DEFAULT CALIBRATION can change on setup
#define CALIBRATEMAX 3300 // DEFAULT CALIBRATION can change on setup
#define MOISTURE_SENSOR_PIN GPIO_NUM_34
#define ADC_CHANNEL ADC1_CHANNEL_6  // Chân ADC1_6 của GPIO34 để đọc giá trị
#define CM_OK 0
#define CM_SIGNAL_ERROR -1


    int getMoisture();    //return humidity as pct
    int         readCM();
    void 	    CM_ErrorHandler(int response);

#endif  
