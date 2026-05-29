################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/DSP/Source/CommonTables/CommonTables.c \
../Drivers/DSP/Source/CommonTables/CommonTablesF16.c \
../Drivers/DSP/Source/CommonTables/arm_common_tables.c \
../Drivers/DSP/Source/CommonTables/arm_common_tables_f16.c \
../Drivers/DSP/Source/CommonTables/arm_const_structs.c \
../Drivers/DSP/Source/CommonTables/arm_const_structs_f16.c \
../Drivers/DSP/Source/CommonTables/arm_mve_tables.c \
../Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.c 

OBJS += \
./Drivers/DSP/Source/CommonTables/CommonTables.o \
./Drivers/DSP/Source/CommonTables/CommonTablesF16.o \
./Drivers/DSP/Source/CommonTables/arm_common_tables.o \
./Drivers/DSP/Source/CommonTables/arm_common_tables_f16.o \
./Drivers/DSP/Source/CommonTables/arm_const_structs.o \
./Drivers/DSP/Source/CommonTables/arm_const_structs_f16.o \
./Drivers/DSP/Source/CommonTables/arm_mve_tables.o \
./Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.o 

C_DEPS += \
./Drivers/DSP/Source/CommonTables/CommonTables.d \
./Drivers/DSP/Source/CommonTables/CommonTablesF16.d \
./Drivers/DSP/Source/CommonTables/arm_common_tables.d \
./Drivers/DSP/Source/CommonTables/arm_common_tables_f16.d \
./Drivers/DSP/Source/CommonTables/arm_const_structs.d \
./Drivers/DSP/Source/CommonTables/arm_const_structs_f16.d \
./Drivers/DSP/Source/CommonTables/arm_mve_tables.d \
./Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/DSP/Source/CommonTables/%.o Drivers/DSP/Source/CommonTables/%.su Drivers/DSP/Source/CommonTables/%.cyclo: ../Drivers/DSP/Source/CommonTables/%.c Drivers/DSP/Source/CommonTables/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DARM_MATH_CM4 -D__FPU_PRESENT=1 -DUSE_HAL_DRIVER -DSTM32G491xx -c -I../Core/Inc -I../Drivers/DSP/ComputeLibrary/Include -I../Drivers/DSP/Include -I../Drivers/DSP/PrivateInclude -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-DSP-2f-Source-2f-CommonTables

clean-Drivers-2f-DSP-2f-Source-2f-CommonTables:
	-$(RM) ./Drivers/DSP/Source/CommonTables/CommonTables.cyclo ./Drivers/DSP/Source/CommonTables/CommonTables.d ./Drivers/DSP/Source/CommonTables/CommonTables.o ./Drivers/DSP/Source/CommonTables/CommonTables.su ./Drivers/DSP/Source/CommonTables/CommonTablesF16.cyclo ./Drivers/DSP/Source/CommonTables/CommonTablesF16.d ./Drivers/DSP/Source/CommonTables/CommonTablesF16.o ./Drivers/DSP/Source/CommonTables/CommonTablesF16.su ./Drivers/DSP/Source/CommonTables/arm_common_tables.cyclo ./Drivers/DSP/Source/CommonTables/arm_common_tables.d ./Drivers/DSP/Source/CommonTables/arm_common_tables.o ./Drivers/DSP/Source/CommonTables/arm_common_tables.su ./Drivers/DSP/Source/CommonTables/arm_common_tables_f16.cyclo ./Drivers/DSP/Source/CommonTables/arm_common_tables_f16.d ./Drivers/DSP/Source/CommonTables/arm_common_tables_f16.o ./Drivers/DSP/Source/CommonTables/arm_common_tables_f16.su ./Drivers/DSP/Source/CommonTables/arm_const_structs.cyclo ./Drivers/DSP/Source/CommonTables/arm_const_structs.d ./Drivers/DSP/Source/CommonTables/arm_const_structs.o ./Drivers/DSP/Source/CommonTables/arm_const_structs.su ./Drivers/DSP/Source/CommonTables/arm_const_structs_f16.cyclo ./Drivers/DSP/Source/CommonTables/arm_const_structs_f16.d ./Drivers/DSP/Source/CommonTables/arm_const_structs_f16.o ./Drivers/DSP/Source/CommonTables/arm_const_structs_f16.su ./Drivers/DSP/Source/CommonTables/arm_mve_tables.cyclo ./Drivers/DSP/Source/CommonTables/arm_mve_tables.d ./Drivers/DSP/Source/CommonTables/arm_mve_tables.o ./Drivers/DSP/Source/CommonTables/arm_mve_tables.su ./Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.cyclo ./Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.d ./Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.o ./Drivers/DSP/Source/CommonTables/arm_mve_tables_f16.su

.PHONY: clean-Drivers-2f-DSP-2f-Source-2f-CommonTables

