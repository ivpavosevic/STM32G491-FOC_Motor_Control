################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.c \
../Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.c 

OBJS += \
./Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.o \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.o 

C_DEPS += \
./Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.d \
./Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/DSP/Source/QuaternionMathFunctions/%.o Drivers/DSP/Source/QuaternionMathFunctions/%.su Drivers/DSP/Source/QuaternionMathFunctions/%.cyclo: ../Drivers/DSP/Source/QuaternionMathFunctions/%.c Drivers/DSP/Source/QuaternionMathFunctions/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM4 -D__FPU_PRESENT=1 -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/DSP/ComputeLibrary/Include -I../Drivers/DSP/Include -I../Drivers/DSP/PrivateInclude -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-DSP-2f-Source-2f-QuaternionMathFunctions

clean-Drivers-2f-DSP-2f-Source-2f-QuaternionMathFunctions:
	-$(RM) ./Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.d ./Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.o ./Drivers/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion2rotation_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_conjugate_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_inverse_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_norm_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_normalize_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_quaternion_product_single_f32.su ./Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.cyclo ./Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.d ./Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.o ./Drivers/DSP/Source/QuaternionMathFunctions/arm_rotation2quaternion_f32.su

.PHONY: clean-Drivers-2f-DSP-2f-Source-2f-QuaternionMathFunctions

