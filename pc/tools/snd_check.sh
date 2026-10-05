#!/bin/sh
# Regenerates every sound artefact and check of docs/pc_port.md section 20 below build/scratch
# (reference waves, every sound rendered, three scripted boots with their dumps, the reports).
# Nothing it writes may be committed (R12).
# Run from the repository root after building build/pc/newschannel.
set -e
N=build/pc/newschannel
S=build/scratch
HBM=6:HomeButton3/Huf8_HomeButtonSe.brsar
mkdir -p $S/waves $S/waves_hbm $S/render $S/render_hbm $S/dumps

$N --dump-waves $S/waves > $S/waves/log.txt
$N --dump-waves $S/waves_hbm $HBM > $S/waves_hbm/log.txt
python3 pc/tools/snd_verify.py waves $S/waves orig/HAGE/contents/09.app > $S/waves/verify.txt

NEWSCHANNEL_AX_LOG=$S/render/ax.log $N --render-sounds $S/render > $S/render/table.txt 2>&1
python3 pc/tools/snd_verify.py render $S/render $S/waves -v > $S/render/verify.txt || true
NEWSCHANNEL_AX_LOG=$S/render_hbm/ax.log $N --render-sounds $S/render_hbm $HBM > $S/render_hbm/table.txt 2>&1
python3 pc/tools/snd_verify.py render $S/render_hbm $S/waves_hbm -v > $S/render_hbm/verify.txt || true

boot() { # name frames nand input
    NEWSCHANNEL_AX_LOG=$S/dumps/$1.log $N --boot --no-window --nand-dir $S/$3 --frames $2 --input "$4" \
        --audio-dump $S/dumps/$1.wav > $S/dumps/$1.out 2>&1 || true
    python3 pc/tools/snd_verify.py stats $S/dumps/$1.wav --ax-log $S/dumps/$1.log > $S/dumps/$1.verify.txt
    python3 pc/tools/snd_verify.py dump $S/dumps/$1.wav $S/dumps/$1.log $S/waves --render-dir $S/render \
        >> $S/dumps/$1.verify.txt || true
}
rm -rf $S/nand_a $S/nand_b
mkdir -p $S/nand_a $S/nand_b
# a: first run. Hover Yes, No, Yes; A on Yes; the connection fails; hover and press "Back to the Wii Menu".
boot boot_a 1300 nand_a "P0:0.08@1,P0.6:0.6@120,P0:0.57@150,P0.6:0.6@200,P0:0.08@230,A@300,P0:0.4@700,P0.8:0.4@760,P0:0.4@800,A@900"
# b: first run. Hover No, A on No.
boot boot_b 700 nand_b "P0.9:0.9@1,P0:0.57@100,A@200"
# c: second run on a's save data: straight to the connection screen, then the error screen's button.
boot boot_c 700 nand_a "P0.9:0.9@1,P0.3:0.3@140,A@150"
echo done
