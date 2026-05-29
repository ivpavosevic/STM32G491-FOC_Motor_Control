################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.c 

OBJS += \
./Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.o 

C_DEPS += \
./Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/DSP/ComputeLibrary/Source/%.o Drivers/DSP/ComputeLibrary/Source/%.su Drivers/DSP/ComputeLibrary/Source/%.cyclo: ../Drivers/DSP/ComputeLibrary/Source/%.c Drivers/DSP/ComputeLibrary/Source/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM4 -D__FPU_PRESENT=1 -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/DSP/ComputeLibrary/Include -I../Drivers/DSP/Include -I../Drivers/DSP/PrivateInclude -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-DSP-2f-ComputeLibrary-2f-Source

clean-Drivers-2f-DSP-2f-ComputeLibrary-2f-Source:
	-$(RM) ./Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.cyclo ./Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.d ./Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.o ./Drivers/DSP/ComputeLibrary/Source/arm_cl_tables.su

.PHONY: clean-Drivers-2f-DSP-2f-ComputeLibrary-2f-Source

