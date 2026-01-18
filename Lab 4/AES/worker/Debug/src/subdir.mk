################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/platform.c \
../src/timer_ps.c \
../src/worker.c 

OBJS += \
./src/platform.o \
./src/timer_ps.o \
./src/worker.o 

C_DEPS += \
./src/platform.d \
./src/timer_ps.d \
./src/worker.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 gcc compiler'
	arm-none-eabi-gcc -Wall -O3 -g3 -c -fmessage-length=0 -MT"$@" -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -std=c99 -mvectorize-with-neon-quad -ftree-vectorize -fopt-info-vec-optimized -fopt-info-vec-missed -ffast-math -I../../worker_bsp/ps7_cortexa9_1/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


