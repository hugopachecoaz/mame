// license:BSD-3-Clause
// copyright-holders:David Haywood, R.Belmont

/*
    These use a 6502 derived CPU under a glob
    The CPU die is marked 'ELAN EU3A14'

    There is a second glob surrounded by TSOP48 pads
    this contains the ROM

    Known to be on this hardware

    Golden Tee Golf Home Edition (developed by FarSight Studios)
    Connectv Football (developed by Medialink)

    Also on this hardware

    name                        PCB ID      ROM width   TSOP pads   ROM size        SEEPROM         die markings

    Real Swing Golf             74037       x16         48          not dumped      no              ELAN EU3A14
    Play TV Basketball          75029       x16         48          not dumped      no              ELAN EU3A14
    Baseball 3                  ?           x16         48          not dumped      no              ELAN EU3A14

    Huntin’3                    ?           x16         48          not dumped      no              Elan ?

    In many ways this is similar to the rad_eu3a05.cpp hardware
    but the video system has changed, here the sprites are more traditional non-tile based, rather
    than coming from 'pages'

*/

#include "emu.h"
#include "cpu/m6502/m6502.h"
#include "screen.h"
#include "speaker.h"
#include "machine/bankdev.h"
#include "audio/rad_eu3a05.h"
#include "machine/timer.h"

/*

ffb0  rti
ffb4  rti
ffb8  rti
ffbc  rti

ffc0  rti
ffc4  rti
ffc8  rti
ffcc  rti

ffd0  rti
ffd4  main irq?
ffd8  rti
ffdc  rti

ffe0  something with 5045 bit 0x08 and 9d in ram (increase or decrease)  (ADC interrupt)
ffe4  something with 5045 bit 0x20 and 9c in ram (increase of decrease)  (ADC interrupt)

ffe8  rti
ffec  rti

regular NMI (e3f0 - jump to ($19e2) which seems to point to rti, but could move..)
regular IRQ (e3f3 - points to rti)

*/


class radica_eu3a14_state : public driver_device
{
public:
	radica_eu3a14_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_palram(*this, "palram"),
		m_scrollregs(*this, "scrollregs"),
		m_tilecfg(*this, "tilecfg"),
		m_tilebase(*this, "tilebase"),
		m_spritebase(*this, "spritebase"),
		m_mainram(*this, "mainram"),
		m_dmaparams(*this, "dmaparams"),
		m_bank(*this, "bank"),
		m_palette(*this, "palette"),
		m_gfxdecode(*this, "gfxdecode")
	{ }

	READ8_MEMBER(irq_vector_r);

	// screen updates
	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

	void radica_eu3a14(machine_config &config);
	void radica_eu3a14_adc(machine_config &config);

	int m_custom_irq;
	uint16_t m_custom_irq_vector;

	INTERRUPT_GEN_MEMBER(interrupt);

	DECLARE_READ8_MEMBER(radicasi_pal_ntsc_r);

	DECLARE_READ8_MEMBER(dma_trigger_r);
	DECLARE_WRITE8_MEMBER(dma_trigger_w);

	DECLARE_READ8_MEMBER(radicasi_rombank_lo_r);
	DECLARE_WRITE8_MEMBER(radicasi_rombank_lo_w);
	DECLARE_WRITE8_MEMBER(radicasi_rombank_hi_w);

	DECLARE_READ8_MEMBER(random_r) { return machine().rand(); };

	TIMER_DEVICE_CALLBACK_MEMBER(scanline_cb);

	// for callback
	DECLARE_READ8_MEMBER(read_full_space);

	void init_rad_gtg();
	void init_rad_foot();

	void bank_map(address_map &map);
	void radica_eu3a14_map(address_map &map);

protected:
	// driver_device overrides
	virtual void machine_start() override;
	virtual void machine_reset() override;

	virtual void video_start() override;

private:
	double hue2rgb(double p, double q, double t);

	required_device<cpu_device> m_maincpu;
	required_shared_ptr<uint8_t> m_palram;
	required_shared_ptr<uint8_t> m_scrollregs;
	required_shared_ptr<uint8_t> m_tilecfg;
	required_shared_ptr<uint8_t> m_tilebase;
	required_shared_ptr<uint8_t> m_spritebase;
	required_shared_ptr<uint8_t> m_mainram;
	required_shared_ptr<uint8_t> m_dmaparams;
	required_device<address_map_bank_device> m_bank;
	required_device<palette_device> m_palette;
	required_device<gfxdecode_device> m_gfxdecode;

	uint8_t m_rombank_hi;
	uint8_t m_rombank_lo;
	int m_tilerambase;
	int m_spriterambase;

	void handle_palette(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void draw_page(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, int which, int xbase, int ybase, int size);
	void draw_background(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
};


void radica_eu3a14_state::video_start()
{
}

double radica_eu3a14_state::hue2rgb(double p, double q, double t)
{
	if (t < 0) t += 1;
	if (t > 1) t -= 1;
	if (t < 1 / 6.0f) return p + (q - p) * 6 * t;
	if (t < 1 / 2.0f) return q;
	if (t < 2 / 3.0f) return p + (q - p) * (2 / 3.0f - t) * 6;
	return p;
}

void radica_eu3a14_state::handle_palette(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	// Palette
	int offs = 0;
	for (int index = 0; index < 512; index++)
	{
		uint16_t dat = m_palram[offs++] << 8;
		dat |= m_palram[offs++];

		// llll lsss ---h hhhh
		int l_raw = (dat & 0xf800) >> 11;
		int sl_raw = (dat & 0x0700) >> 8;
		int h_raw = (dat & 0x001f) >> 0;

		double l = (double)l_raw / 31.0f;
		double s = (double)sl_raw / 7.0f;
		double h = (double)h_raw / 24.0f;

		double r, g, b;

		if (s == 0) {
			r = g = b = l; // greyscale
		}
		else {
			double q = l < 0.5f ? l * (1 + s) : l + s - l * s;
			double p = 2 * l - q;
			r = hue2rgb(p, q, h + 1 / 3.0f);
			g = hue2rgb(p, q, h);
			b = hue2rgb(p, q, h - 1 / 3.0f);
		}

		int r_real = r * 255.0f;
		int g_real = g * 255.0f;
		int b_real = b * 255.0f;

		m_palette->set_pen_color(index, r_real, g_real, b_real);
	}
}

void radica_eu3a14_state::draw_page(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, int which, int xbase, int ybase, int size)
{
	gfx_element *gfx;

	int base = (m_tilebase[1] << 8) | m_tilebase[0];
	if (m_tilecfg[2] & 0x04)
	{
		gfx = m_gfxdecode->gfx(4);
		base <<= 1;

		if (size == 8)
		{
			gfx = m_gfxdecode->gfx(5);
			base <<= 2;
		}
	}
	else
	{
		gfx = m_gfxdecode->gfx(3);
	}

	int xdraw = xbase;
	int ydraw = ybase;
	int count = 0;

	for (int i = m_tilerambase+0x1c0*which; i < m_tilerambase+0x1c0*(which+1); i+=2)
	{
		int tile = m_mainram[i+0] | (m_mainram[i+1] << 8);

		gfx->transpen(bitmap, cliprect, tile+base, 0, 0, 0, xdraw, ydraw, 0);
		xdraw+=size;

		count++;
		if (((count % 16) == 0))
		{
			xdraw -= size*16;
			ydraw += size;
		}
	}
}

void radica_eu3a14_state::draw_background(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	int xscroll = m_scrollregs[0] | (m_scrollregs[1] << 8);
	int yscroll = m_scrollregs[2] | (m_scrollregs[3] << 8);

	int size = 16;
	// or 0x10?
	if (m_tilecfg[0] & 0x80)
	{
		size = 8;
	}

	draw_page(screen, bitmap, cliprect, 0, 0 - xscroll, 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 1, (size * 16) - xscroll, 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 2, 0 - xscroll, (size * 14) - yscroll, size);
	draw_page(screen, bitmap, cliprect, 3, (size * 16) - xscroll, (size * 14) - yscroll, size);

	draw_page(screen, bitmap, cliprect, 0, (size * 16 * 2) + 0 - xscroll, 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 1, (size * 16 * 3) - xscroll, 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 2, (size * 16 * 2) + 0 - xscroll, (size * 14) - yscroll, size);
	draw_page(screen, bitmap, cliprect, 3, (size * 16 * 3) - xscroll, (size * 14) - yscroll, size);

	draw_page(screen, bitmap, cliprect, 0, 0 - xscroll, (size * 14 * 2) + 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 1, (size * 16) - xscroll, (size * 14 * 2) + 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 2, 0 - xscroll, (size * 14 * 3) - yscroll, size);
	draw_page(screen, bitmap, cliprect, 3, (size * 16) - xscroll, (size * 14 * 3) - yscroll, size);

	draw_page(screen, bitmap, cliprect, 0, (size * 16 * 2) + 0 - xscroll, (size * 14 * 2) + 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 1, (size * 16 * 3) - xscroll, (size * 14 * 2) + 0 - yscroll, size);
	draw_page(screen, bitmap, cliprect, 2, (size * 16 * 2) + 0 - xscroll, (size * 14 * 3) - yscroll, size);
	draw_page(screen, bitmap, cliprect, 3, (size * 16 * 3) - xscroll, (size * 14 * 3) - yscroll, size);
}

void radica_eu3a14_state::draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	// first 4 sprite entries seem to be garbage sprites, so we start at 0x20
	// likely we're just interpreting them wrong and they're used for blanking things or clipping?
	for (int i = m_spriterambase; i < m_spriterambase + 0x7e0; i += 8)
	{
		/*
		+0  e--f hhww  flip, enable, height, width
		+1  yyyy yyyy  ypos
		+2  xxxx xxxx  xpos
		+3  pppp ----  palette
		+4  tttt tttt  tile bits
		+5  tttt tttt
		+6  --TT TPPP  TTT = tile bank PPP = bpp select (+more?)
		+7  ---- ----

		*/

		int attr = m_mainram[i + 0];
		int y = m_mainram[i + 1];
		int x = m_mainram[i + 2];
		int attr2 = m_mainram[i + 3];

		int h = attr & 0x0c;
		int w = attr & 0x03;
		int flipx = (attr & 0x10) >> 4;

		int height = 0;
		int width = 0;
		int pal = attr2 >> 4;

		// no idea
		if (attr2 & 0x08)
			pal += 0x10;

		switch (h)
		{
		case 0x0:height = 2; break;
		case 0x4:height = 4; break;
		case 0x8:height = 8; break;
		case 0xc:height = 16; break;
		}

		switch (w)
		{
		case 0x0:width = 1; break;
		case 0x1:width = 2; break;
		case 0x2:width = 4; break;
		case 0x3:width = 8; break;
		}

		y -= ((height * 2) - 4);

		x -= ((width * 4) - 4);

		height *= 4;

		x -= 8;
		y -= 4;

		int offset = ((m_mainram[i + 5] << 8) + (m_mainram[i + 4] << 0));
		int extra = m_mainram[i + 6];
		gfx_element *gfx;
		gfx = m_gfxdecode->gfx(1);

		int spritebase = (m_spritebase[1] << 8) | m_spritebase[0];

		offset += (extra & 0xf8) << 13;
		extra &= ~0xf8;
		offset += spritebase << 7;

		switch (extra & 0x07)
		{
		case 0x00: // 8bpp
		case 0x07: // 8bpp
			offset >>= 1;
			gfx = m_gfxdecode->gfx(2);
			break;

		case 0x02: // 2bpp
			offset <<= 1;
			gfx = m_gfxdecode->gfx(0);
			pal = 0;
			break;

		case 0x04: // 4bpp
			break;

		case 0x01: // unknowns
		case 0x03:
		case 0x05:
		case 0x06:
			pal = machine().rand();
			break;
		}

		offset = offset >> 1;

		if (attr & 0x80)
		{
			int count = 0;
			for (int yy = 0; yy < height; yy++)
			{
				for (int xx = 0; xx < width; xx++)
				{
					gfx->transpen(bitmap, cliprect, offset + count, pal, flipx, 0, x + xx * 8, y + yy, 0);
					count++;
				}
			}
		}
	}
}



uint32_t radica_eu3a14_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	bitmap.fill(0, cliprect);

	handle_palette(screen, bitmap, cliprect);
	draw_background(screen, bitmap, cliprect);
	draw_sprites(screen, bitmap, cliprect);

	return 0;
}

// sound callback
READ8_MEMBER(radica_eu3a14_state::read_full_space)
{
	address_space& fullbankspace = m_bank->space(AS_PROGRAM);
	return fullbankspace.read_byte(offset);
}

// irq controller seems to be like the Radica Space Invaders
READ8_MEMBER(radica_eu3a14_state::irq_vector_r)
{
	if (m_custom_irq)
	{
		return m_custom_irq_vector >> (offset*8);
	}
	else
	{
		uint8_t *rom = memregion("maincpu")->base();
		return rom[0x001ffe + offset];
	}
}

/*
   code at 0000 maps to e000
   code at 1000 maps to f000

   data at 2000
   data at 3000
   data at 4000
   blank   5000
   blank   6000

   code at 7000 maps to 3000
   code at 8000 maps to 6000
           9000 maps to 7000
           a000 maps to 8000
           b000 maps to 9000
           c000 maps to a000
           d000 maps to b000
           e000 maps to c000
*/

WRITE8_MEMBER(radica_eu3a14_state::radicasi_rombank_hi_w)
{
	// written with the banking?
	//logerror("%s: radicasi_rombank_hi_w (set ROM bank) %02x\n", machine().describe_context(), data);
	m_rombank_hi = data;

	m_bank->set_bank(m_rombank_lo | (m_rombank_hi << 8));
}

WRITE8_MEMBER(radica_eu3a14_state::radicasi_rombank_lo_w)
{
	//logerror("%s: radicasi_rombank_lo_w (select ROM bank) %02x\n", machine().describe_context(), data);
	m_rombank_lo = data;
}

READ8_MEMBER(radica_eu3a14_state::radicasi_rombank_lo_r)
{
	return m_rombank_lo;
}

READ8_MEMBER(radica_eu3a14_state::radicasi_pal_ntsc_r)
{
	// how best to handle this, we probably need to run the PAL machine at 50hz
	// the text under the radica logo differs between regions
	logerror("%s: radicasi_pal_ntsc_r (region + more?)\n", machine().describe_context());
	return 0xff; // NTSC
	//return 0x00; // PAL
}

void radica_eu3a14_state::bank_map(address_map &map)
{
	map(0x000000, 0x3fffff).rom().region("maincpu", 0);
}

void radica_eu3a14_state::radica_eu3a14_map(address_map &map)
{
	map(0x0000, 0x01ff).ram();
	map(0x0200, 0x3fff).ram().share("mainram"); // 200-9ff is sprites? a00 - ??? is tilemap?

	map(0x4800, 0x4bff).ram().share("palram");

	// similar to eu3a05, at least for pal flags and rom banking
	map(0x5007, 0x5007).noprw();
	map(0x5008, 0x5008).nopw(); // startup
	map(0x5009, 0x5009).noprw();
	map(0x500a, 0x500a).nopw(); // startup
	map(0x500b, 0x500b).r(this, FUNC(radica_eu3a14_state::radicasi_pal_ntsc_r)).nopw(); // PAL / NTSC flag at least
	map(0x500c, 0x500c).w(this, FUNC(radica_eu3a14_state::radicasi_rombank_hi_w));
	map(0x500d, 0x500d).rw(this, FUNC(radica_eu3a14_state::radicasi_rombank_lo_r), FUNC(radica_eu3a14_state::radicasi_rombank_lo_w));

	// DMA is similar to, but not the same as eu3a05
	map(0x500f, 0x5017).ram().share("dmaparams");
	map(0x5018, 0x5018).rw(this, FUNC(radica_eu3a14_state::dma_trigger_r), FUNC(radica_eu3a14_state::dma_trigger_w));

	// probably GPIO like eu3a05, although it access 47/48 as unknown instead of 48/49/4a
	map(0x5040, 0x5040).nopw();
	map(0x5041, 0x5041).portr("IN0");
	map(0x5042, 0x5042).nopw();
	map(0x5043, 0x5043).noprw();
	map(0x5044, 0x5044).nopw();
	map(0x5045, 0x5045).portr("IN1").nopw();
	map(0x5046, 0x5046).nopw();
	map(0x5047, 0x5047).nopw();
	map(0x5048, 0x5048).nopw();

	// sound appears to be the same as eu3a05
	map(0x5080, 0x5091).rw("6ch_sound", FUNC(radica6502_sound_device::radicasi_sound_addr_r), FUNC(radica6502_sound_device::radicasi_sound_addr_w));
	map(0x5092, 0x50a3).rw("6ch_sound", FUNC(radica6502_sound_device::radicasi_sound_size_r), FUNC(radica6502_sound_device::radicasi_sound_size_w));
	map(0x50a4, 0x50a4).rw("6ch_sound", FUNC(radica6502_sound_device::radicasi_sound_unk_r), FUNC(radica6502_sound_device::radicasi_sound_unk_w)); // read frequently on this
	map(0x50a5, 0x50a5).rw("6ch_sound", FUNC(radica6502_sound_device::radicasi_sound_trigger_r), FUNC(radica6502_sound_device::radicasi_sound_trigger_w));
	map(0x50a6, 0x50a6).nopw(); // startup
	map(0x50a7, 0x50a7).nopw(); // startup
	map(0x50a8, 0x50a8).r("6ch_sound", FUNC(radica6502_sound_device::radicasi_50a8_r));
	map(0x50a9, 0x50a9).nopw(); // startup

	// video regs are here this time
	map(0x5100, 0x5100).ram();
	map(0x5103, 0x5106).ram();
	map(0x5107, 0x5107).ram(); // on transitions, maybe layer disables?

	map(0x5110, 0x5112).ram().share("tilecfg");
	map(0x5113, 0x5113).ram(); // written with tilebase?
	map(0x5114, 0x5115).ram().share("tilebase");
	map(0x5116, 0x5117).ram();
	map(0x5121, 0x5124).ram().share("scrollregs");
	map(0x5150, 0x5150).ram(); // startup
	map(0x5151, 0x5152).ram().share("spritebase");
	map(0x5153, 0x5153).ram(); // startup

	map(0x6000, 0xdfff).m(m_bank, FUNC(address_map_bank_device::amap8));

	map(0xe000, 0xffff).rom().region("maincpu", 0x0000);

	map(0xfffe, 0xffff).r(this, FUNC(radica_eu3a14_state::irq_vector_r));
}

READ8_MEMBER(radica_eu3a14_state::dma_trigger_r)
{
	logerror("%s: dma_trigger_r\n", machine().describe_context());
	return 0;
}

WRITE8_MEMBER(radica_eu3a14_state::dma_trigger_w)
{
	uint32_t dmasrc = (m_dmaparams[2] << 16) | (m_dmaparams[1] << 8) | (m_dmaparams[0] << 0);
	uint32_t dmadst = (m_dmaparams[5] << 16) | (m_dmaparams[4] << 8) | (m_dmaparams[3] << 0);
	uint32_t dmalen = (m_dmaparams[8] << 16) | (m_dmaparams[7] << 8) | (m_dmaparams[6] << 0);

	//logerror("%s: dma_trigger_w %02x (src %08x dst %08x size %08x)\n", machine().describe_context(), data, dmasrc, dmadst, dmalen);

	address_space& fullbankspace = m_bank->space(AS_PROGRAM);
	address_space& destspace = m_maincpu->space(AS_PROGRAM);

	if (data == 0x08)
	{
		for (int i = 0; i < dmalen; i++)
		{
			uint8_t dat = fullbankspace.read_byte(dmasrc + i);
			destspace.write_byte(dmadst + i, dat);
		}
	}
	else
	{
		logerror("UNKNOWN DMA TRIGGER VALUE\n");
	}
}


// hold back/backspin and left during power on for test mode
static INPUT_PORTS_START( rad_gtg )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) // back / backspin
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) // up and down in the menus should be the trackball, maybe these are leftovers from real swing golf or just from development?
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )

	PORT_START("IN1")
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x08, 0x08, "Track Y test" ) // trackball up/down direction bit? (read in interrupt, increases / decreases a counter)
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x20, 0x20, "Track X test" ) // trackball left / right direction bit? (read in interrupt, increases / decreases a counter)
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

// hold enter and left during power on for test mode
static INPUT_PORTS_START( radica_eu3a14 )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 ) // enter?
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("IN1")
	PORT_DIPNAME( 0x01, 0x01, "IN1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

void radica_eu3a14_state::machine_start()
{
}

void radica_eu3a14_state::machine_reset()
{
	// rather be safe
	m_maincpu->set_state_int(M6502_S, 0x1ff);

	m_bank->set_bank(0x01);
}


TIMER_DEVICE_CALLBACK_MEMBER(radica_eu3a14_state::scanline_cb)
{
	// these interrupts need to occur based on how fast the trackball is
	// being moved, the direction is read in a port.

	int scanline = param;

	if (scanline == 20)
	{
		// vertical trackball
		m_custom_irq_vector = 0xffe0;
		m_maincpu->set_input_line(INPUT_LINE_IRQ0,HOLD_LINE);
	}

	if (scanline == 40)
	{
		// horizontal trackball
		m_custom_irq_vector = 0xffe4;
		m_maincpu->set_input_line(INPUT_LINE_IRQ0,HOLD_LINE);
	}
}

INTERRUPT_GEN_MEMBER(radica_eu3a14_state::interrupt)
{
	m_custom_irq = 1;
	m_custom_irq_vector = 0xffd4;
	m_maincpu->set_input_line(INPUT_LINE_IRQ0,HOLD_LINE);
}


static const gfx_layout helper8x1x2_layout =
{
	8,1,
	RGN_FRAC(1,1),
	2,
	{ STEP2(0,1) },
	{ STEP8(0,2) },
	{ 0 },
	8 * 2
};

static const gfx_layout helper8x1x4_layout =
{
	8,1,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP8(0,4) },
	{ 0 },
	8 * 4
};

static const gfx_layout helper8x1x8_layout =
{
	8,1,
	RGN_FRAC(1,1),
	8,
	{ STEP8(0,1) },
	{ STEP8(0,8) },
	{ 0 },
	8 * 8
};

// background
static const gfx_layout helper16x16x8_layout =
{
	16,16,
	RGN_FRAC(1,1),
	8,
	{ STEP8(0,1) },
	{ STEP16(0,8) },
	{ STEP16(0,16*8)  },
	16 * 16 * 8
};

static const gfx_layout helper16x16x4_layout =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP16(0,4) },
	{ STEP16(0,16*4)  },
	16 * 16 * 4
};

static const gfx_layout helper8x8x4_layout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP8(0,4) },
	{ STEP8(0,8*4)  },
	8 * 8 * 4
};


static GFXDECODE_START( gfx_helper )
	GFXDECODE_ENTRY( "maincpu", 0, helper8x1x2_layout,    0x0, 128  )
	GFXDECODE_ENTRY( "maincpu", 0, helper8x1x4_layout,    0x0, 32  )
	GFXDECODE_ENTRY( "maincpu", 0, helper8x1x8_layout,    0x0, 2  )
	GFXDECODE_ENTRY( "maincpu", 0, helper16x16x8_layout,  0x0, 2  )
	GFXDECODE_ENTRY( "maincpu", 0, helper16x16x4_layout,  0x0, 32  )
	GFXDECODE_ENTRY( "maincpu", 0, helper8x8x4_layout,    0x0, 32  )
GFXDECODE_END



MACHINE_CONFIG_START(radica_eu3a14_state::radica_eu3a14)
	/* basic machine hardware */
	MCFG_DEVICE_ADD("maincpu",M6502,XTAL(21'477'272)/2) // marked as 21'477'270
	MCFG_DEVICE_PROGRAM_MAP(radica_eu3a14_map)
	MCFG_DEVICE_VBLANK_INT_DRIVER("screen", radica_eu3a14_state,  interrupt)

	MCFG_DEVICE_ADD("bank", ADDRESS_MAP_BANK, 0)
	MCFG_DEVICE_PROGRAM_MAP(bank_map)
	MCFG_ADDRESS_MAP_BANK_ENDIANNESS(ENDIANNESS_LITTLE)
	MCFG_ADDRESS_MAP_BANK_DATA_WIDTH(8)
	MCFG_ADDRESS_MAP_BANK_ADDR_WIDTH(24)
	MCFG_ADDRESS_MAP_BANK_STRIDE(0x8000)

	MCFG_DEVICE_ADD("gfxdecode", GFXDECODE, "palette", gfx_helper)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500))
	MCFG_SCREEN_UPDATE_DRIVER(radica_eu3a14_state, screen_update)
	MCFG_SCREEN_SIZE(32*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 0*8, 28*8-1)

	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD("palette", 512)

	/* sound hardware */
	SPEAKER(config, "mono").front_center();
	MCFG_DEVICE_ADD("6ch_sound", RADICA6502_SOUND, 8000)
	MCFG_RADICA6502_SOUND_SPACE_READ_CB(READ8(*this, radica_eu3a14_state, read_full_space))
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)

MACHINE_CONFIG_END

MACHINE_CONFIG_START(radica_eu3a14_state::radica_eu3a14_adc)
	radica_eu3a14(config);

	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", radica_eu3a14_state, scanline_cb, "screen", 0, 1)
MACHINE_CONFIG_END

void radica_eu3a14_state::init_rad_gtg()
{
	// must be registers to control this
	m_tilerambase = 0x0a00 - 0x200;
	m_spriterambase = 0x0220 - 0x200;
}

void radica_eu3a14_state::init_rad_foot()
{
	// must be registers to control this
	m_tilerambase = 0x0200 - 0x200;
	m_spriterambase = 0x2800 - 0x200;
}


ROM_START( rad_gtg )
	ROM_REGION( 0x400000, "maincpu", ROMREGION_ERASE00 )
	ROM_LOAD16_WORD_SWAP( "goldentee.bin", 0x000000, 0x400000, CRC(b1985c63) SHA1(c42a59fcb665eb801d9ca5312b90e39333e52de4) )
ROM_END

ROM_START( rad_foot )
	ROM_REGION( 0x400000, "maincpu", ROMREGION_ERASE00 )
	ROM_LOAD( "connectvfootball.bin", 0x000000, 0x400000, CRC(00ac4fc0) SHA1(2b60ae5c6bc7e9ef7cdbd3f6a0a0657ed3ab5afe) )
ROM_END

CONS( 2006, rad_gtg,  0, 0, radica_eu3a14_adc, rad_gtg,       radica_eu3a14_state, init_rad_gtg,  "Radica (licensed from Incredible Technologies)", "Golden Tee Golf: Home Edition", MACHINE_NOT_WORKING )

// also has a Connectv Real Soccer logo in the roms, apparently unused, maybe that was to be the US title (without the logo being changed to Play TV) but Play TV Soccer ended up being a different game licensed from Epoch instead.
CONS( 2006, rad_foot, 0, 0, radica_eu3a14,     radica_eu3a14, radica_eu3a14_state, init_rad_foot, "Radica", "Connectv Football", MACHINE_NOT_WORKING )
