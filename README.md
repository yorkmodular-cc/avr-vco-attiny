This is a redesign of the old 3HP VCO to make it friendlier to those people
who are running systems built into stupidly shallow cases, but there are a
few differences as well.

If you want an all-singing, all-dancing VCO with lots of bells, whistles and
foghorns well, there are plenty of them on the market so feel free to go take
a look at them. However, if you want something simple and hackable then we've
got you covered.

Analogue VCO cores are a pain to build so this is based around a
microcontroller instead - cheating? Maybe so, but it's pretty versatile and
if you have the knowledge to program microcontrollers then you can hack it to
your heart's content.

As supplied, the module can provide six different output waveforms: sine,
triangle, square, sawtooth, something pulse-ish and digital noise - waveform
selection is performed via the 'Wave' pot. If you want to get down and dirty
with the code then you could easily replace the wavetables with something else.

It'll track 1V/octave pretty well over 5 octaves, this being governed by the
fact that microcontrollers tend to get a bit upset if you feed more than 5V
to their input pins.

The firmware source has a permissive license, so feel free to hack on it -
updates and modifications are most welcome.

The microcontroller VCO core is present and correct, and it uses some tried
and tested code to track 1V/octave over about 5 octaves. However, there's now
a dedicated fine-tune control and the waveform is no longer voltage controlled.

Not only that, but there's also a range switch which allows you to use the
module as either a 'regular' VCO with a range starting at C1 (~32Hz) or
something more akin to an LFO, again with a range of around 5 octaves and
1V/oct scaling.

The output waveform is a filtered PWM signal - the output filter has a cutoff
frequency of 1.6kHz which gives you a usable range of 5 octaves - it's a lot
cheaper than a DAC.

**CURRENT DRAW (approx.):** ~25mA (+12). Does not use -12V.
