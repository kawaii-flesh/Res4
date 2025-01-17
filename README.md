# Res4 \[ˈresə\]

Res4 основана на [KipTool](https://github.com/kawaii-flesh/KipTool) v8.1.4-26

Выполняет сверку контрольных сумм (sha256) и заданный набор команд 

## Установка и запуск
1. Поместить `Res4.bin` в `sd:/bootloader/payloads`
2. Поместить `sha256.list`, `post.script` и `sha256_post.list` (необязательный файл) в `sd:/res4`
3. Загрузиться в `Hekate`
4. Загрузить пейлоад: `Payloads` -> `Res4.bin`

## Назначение файлов
`sha256.list` - содержит список контрольных сумм, которые необходимо проверить перед запуском `post.script`

`post.script` - набор команд, выполняемый в случае успешной проверки контрольных сумм из `sha256.list`

`sha256_post.list` (необязательный файл) - содержит список контрольных сумм, которые необходимо проверить в случае успешного выполнения `post.script`


### Формат sha256.list и sha256_post.list
`контрольная сумма`|`смещение 10 мегабайтного блока в файле`|`путь к файлу`
```
1113c64365042102ca426...1c03a6db6ec8859a7cd7424|0|/bootloader/payloads/Res4.bin
3d3f80a5e00ad9e23e11d...e963d0df9f346eb3670575b|0|/res4/4IFIR.tar
92a6ec1879ef78493f776...accafa228043ea8c56665fe|10485760|/res4/4IFIR.tar
```
Для создания `sha256.list` можно воспользоваться скриптом `sha256.py`
```bash
python3 sha256.py файл1 файл2 ...
```

### Формат post.script
`post.script` состоит из набора команд следующего формата
`команда`|`аргумент1`|`аргумент2`|`...`

Доступные команды:
- `rm`|`path` - удаляет директорию/файл по указанному пути
- `cp`|`path from`|`path to` - копирует директорию/файл `path from` в `path to`
- `extract`|`tar path`|`path to` - извлекает содержимое tar архива по `tar path` в `path to`

Дополнительно:
- С помощью `~` можно сделать успех выполнения команды необязательным для продолжения выполнения
- С помощью `?` можно вызвать диалоговое окно соглашения на выполнение команды

Пример:
```
rm|/atmosphere
rm|/boot.dat
rm|/boot.ini
rm|/bootloader
rm|/config
rm|/exosphere.ini
~rm|/games
rm|/hbmenu.nro
rm|/payload.bin
rm|/SaltySD
rm|/switch
rm|/warmboot_mariko
extract|/res4/4IFIR.tar|/
?cp|/res4/Res4.bin|/bootloader/payloads/Res4.bin
```

## Скриншоты

![1](/screenshots/1.jpg)

![2](/screenshots/2.jpg)

![3](/screenshots/3.jpg)
