This is a beta version of RTL1090.
Functionality is experimental and partially incomplete yet.
Please use at your own risk.
Retain the release that is working for you as a copy.

============================================================================
Build 103 - 15/09/2013

Added a switch that will enable/disable the FEC function
By default FEC is disabled in this version!
You can enable FEC by either
- check a new box in the CONFIG dialog - or -
- open the app with command line entry "/fec"

Note that - as always with RTL1090 - command line entries are not reflected 
in the CONFIG dialog.
Also, you will note that switching off FEC will reduce the cycle time 
(ms in the lower left corner).

============================================================================
Build 102 - 29/06/2013

- Decoder improved by brute force 1-bit error correction and brute force (FEC)

 DF11 and DF17 override for unknown DFs. This may be tough stuff
for slow computers. If your computer slows down considerably return
to a previous version please.

- Tabs introduced - List/Table selection moved to Tabs.
- Stats tab added: this brings back the previous status bar info about USB
packets per second.

- II/SI tab added. This tab adds the ability to quickly scan the Mode-S
interrogator codes around. A "radar1090.txt" file can be added to the
rtl1090 folder that holds II/SI code decodings. A sample file is attached.
The II/SI collection can be reset by a right mouse click action.
The recently seen codes and the last aircraft responding with that
code are displayed in red onwhite.

- Context menu (right mouse click) for text window, "listhold" established
by menu selection, revoked by mouse click

============================================================================
Build 101 - 20/05/2013

- GUI completely refurbished. Program window height can be altered and switches
are hidden by default. This makes the GUI more space efficient (and it looks
better)

- In status line: USB packet indicator (packets/s) changed to queue cycle
time (ms). The queue cycle time must stay below 300 ms for
lossless processing.

- Packet rate LEDs show all red when it appears that a USB data frame
 was lost. This is in preparation for work on better MLAT accuracy.

- Avg. Signal Strength (SG) and Message count (MSGS) added to flight table.
Avg. Signal Strength is normalized to 100% (max 99%).
Color codes are yellow (you can hear it), orange (you can see it),
red (you can touch it).

- Default display order in flight table changed. Newest flights are at top
for compatibility with DUMP1090 (not completely working yet)

- CONFIG dialog: some commmand line options can be selected from the dialog.
Command line entries are still valid and do override any selections.
If overriding occurs entries are printed in bold, but boxes may not be checked,
so the override conditions will not be saved to the configuration file.

- Update alert: when a new version is available an update sign appears in the
caption bar - no further functions yet

============================================================================


Это бета-версия RTL1090.
Функциональность является экспериментальной и частично еще не завершена.
Пожалуйста используйте на свой страх и риск.
Сохраните рабочий релиз в качестве копии.

================================================== ===========================
Сборка 103 - 15.09.2013

Добавлен переключатель, который включает/отключает функцию FEC.
По умолчанию в этой версии FEC отключен!
Вы можете включить FEC либо
- установите новый флажок в диалоговом окне CONFIG - или -
- откройте приложение с помощью ввода командной строки "/fec"

Обратите внимание, что, как всегда с RTL1090, записи командной строки не отображаются.
в диалоговом окне КОНФИГУРАЦИЯ.
Также вы заметите, что отключение FEC сократит время цикла.
(мс в левом нижнем углу).

================================================== ===========================
Сборка 102 - 29.06.2013

- Декодер улучшен перебором 1-битной коррекции ошибок и перебором
  (ФЭК)

  DF11 и DF17 переопределяют неизвестные DF. Это может быть жесткой вещью
для медленных компьютеров. Если ваш компьютер сильно тормозит, вернитесь
на предыдущую версию пожалуйста.

- Введены вкладки. Выбор списка/таблицы перемещен во вкладки.
- Добавлена вкладка «Статистика»: она возвращает предыдущую информацию в строке состояния о USB.
пакетов в секунду.

- Добавлена вкладка II/SI. Эта вкладка добавляет возможность быстрого сканирования Mode-S
Коды следователя вокруг. Файл "radar1090.txt" может быть добавлен к
rtl1090, в которой хранятся расшифровки кода II/SI. Образец файла прилагается.
Коллекцию II/SI можно сбросить, щелкнув правой кнопкой мыши.
Недавно увиденные коды и последний самолет, ответивший этим
код отображаются красным на белом.

- Контекстное меню (щелчок правой кнопкой мыши) для текстового окна, установлен "listhold"
выбором меню, отменяется щелчком мыши

================================================== ===========================
Сборка 101 - 20.05.2013

- Полностью переработан графический интерфейс. Высота окна программы может изменяться и переключаться
по умолчанию скрыты. Это делает графический интерфейс более эффективным с точки зрения использования пространства (и выглядит
лучше)

- В строке состояния: индикатор USB-пакетов (пакетов/с) изменен на цикл очереди.
время (мс). Время цикла очереди должно оставаться ниже 300 мс для
обработка без потерь.

- Индикаторы скорости передачи пакетов загораются красным, когда кажется, что кадр данных USB
  был потерян. Это подготовка к работе над повышением точности MLAT.

- Сред. Сила сигнала (SG) и количество сообщений (MSGS) добавлены в таблицу полетов.
Сред. Сила сигнала нормализована до 100% (макс. 99%).
Цветовые коды: желтый (вы можете это услышать), оранжевый (вы можете это увидеть),
красный (вы можете потрогать его).

- Изменен порядок отображения по умолчанию в таблице полетов. Самые новые рейсы вверху
для совместимости с DUMP1090 (пока не полностью рабочий)

- Диалоговое окно CONFIG: в диалоговом окне можно выбрать некоторые параметры командной строки.
Записи командной строки по-прежнему действительны и переопределяют любой выбор.
Если происходит переопределение, записи выделяются жирным шрифтом, но флажки могут быть не отмечены.
поэтому условия переопределения не будут сохранены в файле конфигурации.

- Предупреждение об обновлении: когда доступна новая версия, в
строка заголовка - никаких дополнительных функций пока нет