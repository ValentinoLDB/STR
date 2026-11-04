$ErrorActionPreference = "Stop"

$avrBin  = "C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\bin"
$dfpInc  = "C:\Program Files (x86)\Atmel\Studio\7.0\Packs\atmel\ATmega_DFP\1.6.364\include"
$dfpDev  = "C:\Program Files (x86)\Atmel\Studio\7.0\Packs\atmel\ATmega_DFP\1.6.364\gcc\dev\atmega328p"
$srcDir  = "E:\Sistemas de Tiempo Real\Practicas\Practica3\Firmware\Practica3"
$dbgDir  = "$srcDir\Debug"
$binDir  = "E:\Sistemas de Tiempo Real\Practicas\Practica3\Firmware\Binarios"

$gcc     = "$avrBin\avr-gcc.exe"
$objcopy = "$avrBin\avr-objcopy.exe"
$size    = "$avrBin\avr-size.exe"

$sources = @("heap_1.c", "list.c", "main.c", "port.c", "queue.c", "tasks.c")

function Build-Variant($outName, $extraDefines) {
    Write-Host "=== Compilando $outName ($extraDefines) ==="
    $objs = @()
    foreach ($s in $sources) {
        $base = [System.IO.Path]::GetFileNameWithoutExtension($s)
        $obj  = "$dbgDir\$base.o"
        $argsList = @(
            "-x", "c", "-funsigned-char", "-funsigned-bitfields", "-DDEBUG"
        ) + $extraDefines + @(
            "-I$dfpInc", "-I$srcDir\FreeRTOS\include", "-I$srcDir\FreeRTOS", "-I$srcDir",
            "-Og", "-ffunction-sections", "-fdata-sections", "-fpack-struct", "-fshort-enums",
            "-g2", "-Wall", "-mmcu=atmega328p", "-B", $dfpDev, "-c", "-std=gnu99",
            "-o", $obj, "$srcDir\$s"
        )
        & $gcc @argsList
        if ($LASTEXITCODE -ne 0) { throw "Error compilando $s" }
        $objs += $obj
    }

    $elf = "$binDir\$outName.elf"
    $hex = "$binDir\$outName.hex"
    $linkArgs = @("-o", $elf) + $objs + @(
        "-Wl,--gc-sections", "-mmcu=atmega328p", "-B", $dfpDev, "-lm"
    )
    & $gcc @linkArgs
    if ($LASTEXITCODE -ne 0) { throw "Error enlazando $outName" }

    & $objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures $elf $hex
    & $size -C --mcu=atmega328p $elf
}

Build-Variant "Ej3b_Calibracion_TareaD" @("-DMODO_CALIBRACION_EJ3=1", "-DconfigUSE_TIME_SLICING=1")
Build-Variant "Ej5_FIFO"                @("-DMODO_CALIBRACION_EJ3=0", "-DconfigUSE_TIME_SLICING=0")
Build-Variant "Ej4_RoundRobin"          @("-DMODO_CALIBRACION_EJ3=0", "-DconfigUSE_TIME_SLICING=1")

Copy-Item -Force "$binDir\Ej4_RoundRobin.elf" "$dbgDir\Practica3.elf"
Copy-Item -Force "$binDir\Ej4_RoundRobin.hex" "$dbgDir\Practica3.hex"
Write-Host "Compilacion exitosa de las 3 variantes."
