################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/DSP/Source/BayesFunctions/BayesFunctions.c \
../Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.c \
../Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.c \
../Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.c 

OBJS += \
./Drivers/DSP/Source/BayesFunctions/BayesFunctions.o \
./Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.o \
./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.o \
./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.o 

C_DEPS += \
./Drivers/DSP/Source/BayesFunctions/BayesFunctions.d \
./Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.d \
./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.d \
./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/DSP/Source/BayesFunctions/%.o Drivers/DSP/Source/BayesFunctions/%.su Drivers/DSP/Source/BayesFunctions/%.cyclo: ../Drivers/DSP/Source/BayesFunctions/%.c Drivers/DSP/Source/BayesFunctions/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM4 -D__FPU_PRESENT=1 -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/DSP/ComputeLibrary/Include -I../Drivers/DSP/Include -I../Drivers/DSP/PrivateInclude -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-DSP-2f-Source-2f-BayesFunctions

clean-Drivers-2f-DSP-2f-Source-2f-BayesFunctions:
	-$(RM) ./Drivers/DSP/Source/BayesFunctions/BayesFunctions.cyclo ./Drivers/DSP/Source/BayesFunctions/BayesFunctions.d ./Drivers/DSP/Source/BayesFunctions/BayesFunctions.o ./Drivers/DSP/Source/BayesFunctions/BayesFunctions.su ./Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.cyclo ./Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.d ./Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.o ./Drivers/DSP/Source/BayesFunctions/BayesFunctionsF16.su ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.cyclo ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.d ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.o ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f16.su ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.cyclo ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.d ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.o ./Drivers/DSP/Source/BayesFunctions/arm_gaussian_naive_bayes_predict_f32.su

.PHONY: clean-Drivers-2f-DSP-2f-Source-2f-BayesFunctions

