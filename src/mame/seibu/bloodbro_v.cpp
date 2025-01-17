// license:BSD-3-Clause
// copyright-holders:Carlos A. Lozano
/***************************************************************************

    Video Hardware for Blood Brothers

    Note:
    - An extra layer is currently disabled via the layer enable register
      in Seibu CRTC device (bit 2). Is it even tied to any RAM portion?

***************************************************************************/

#include "emu.h"
#include "bloodbro.h"
#include "screen.h"


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

TILE_GET_INFO_MEMBER(bloodbro_state::get_bg_tile_info)
{
	int code = m_bgvideoram[tile_index];
	tileinfo.set(1,
			code & 0xfff,
			(code >> 12),
			0);
}

TILE_GET_INFO_MEMBER(bloodbro_state::get_fg_tile_info)
{
	int code = m_fgvideoram[tile_index];
	tileinfo.set(2,
			(code & 0xfff)+0x1000,
			(code >> 12),
			0);
}

TILE_GET_INFO_MEMBER(bloodbro_state::get_tx_tile_info)
{
	int code = m_txvideoram[tile_index];
	tileinfo.set(0,
			code & 0xfff,
			code >> 12,
			0);
}



/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

void bloodbro_state::video_start()
{
	m_bg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(*this, FUNC(bloodbro_state::get_bg_tile_info)), TILEMAP_SCAN_ROWS, 16, 16, 32, 16);
	m_fg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(*this, FUNC(bloodbro_state::get_fg_tile_info)), TILEMAP_SCAN_ROWS, 16, 16, 32, 16);
	m_tx_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(*this, FUNC(bloodbro_state::get_tx_tile_info)), TILEMAP_SCAN_ROWS,  8,  8, 32, 32);

	m_fg_tilemap->set_transparent_pen(15);
	m_tx_tilemap->set_transparent_pen(15);

	save_item(NAME(m_scrollram));
	save_item(NAME(m_layer_en));
}



/***************************************************************************

  Memory handlers

***************************************************************************/

void bloodbro_state::bgvideoram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_bgvideoram[offset]);
	m_bg_tilemap->mark_tile_dirty(offset);
}

void bloodbro_state::fgvideoram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_fgvideoram[offset]);
	m_fg_tilemap->mark_tile_dirty(offset);
}

void bloodbro_state::txvideoram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_txvideoram[offset]);
	m_tx_tilemap->mark_tile_dirty(offset);
}



/***************************************************************************

  Display refresh


    Blood Bros / Skysmash Spriteram
    -------------------------------

    Slightly more sophisticated successor to the Toki sprite chip.

    It has "big sprites" created by setting width or height >0. Tile
    numbers are read consecutively.

    +0   x....... ........  sprite disabled if set
    +0   .x...... ........  Flip y (no evidence for this!!)
    +0   ..x..... ........  Flip x
    +0   ....x... ........  Priority (1=high)
    +0   ......xx x.......  Width: do this many tiles horizontally
    +0   ........ .xxx....  Height: do this many tiles vertically
    +0   ........ ....xxxx  Color bank

    +1   ...xxxxx xxxxxxxx  Tile number
    +2   .......x xxxxxxxx  X coordinate
    +3   .......x xxxxxxxx  Y coordinate


    Weststry Bootleg Spriteram
    --------------------------

    Lacks the "big sprite" feature of the original. Needs some
    tile number remapping for some reason.

    +0   .......x xxxxxxxx  Sprite Y coordinate
    +1   ...xxxxx xxxxxxxx  Sprite tile number
    +2   xxxx.... ........  Sprite color bank
    +2   ......x. ........  Sprite flip x
    +2   ........ x.......  Priority ??
    +3   .......x xxxxxxxx  Sprite X coordinate

***************************************************************************/

/* SPRITE INFO (8 bytes)

   D-F?P?SS SSSSCCCC
   ---TTTTT TTTTTTTT
   -------X XXXXXXXX
   -------- YYYYYYYY */

uint32_t bloodbro_state::pri_cb(uint8_t pri, uint8_t ext)
{
	return pri ? 0x02 : 0;
}

/* SPRITE INFO (8 bytes)

   D------- YYYYYYYY
   ---TTTTT TTTTTTTT
   CCCC--F? -?--????  Priority??
   -------X XXXXXXXX
*/

void weststry_state::weststry_draw_sprites(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint16_t *spriteram16 = m_spriteram;
	int offs;

	for (offs = m_spriteram.bytes()/2 - 4; offs >= 0; offs -= 4)
	{
		int data = spriteram16[offs+2];
		int data0 = spriteram16[offs+0];
		int code = spriteram16[offs+1]&0x1fff;
		int sx = spriteram16[offs+3]&0x1ff;
		int sy = 0xf0-(data0&0xff);
		int flipx = data & 0x200;
		int flipy = data & 0x400;   /* ??? */
		int color = (data&0xf000)>>12;
		int pri_mask = (data & 0x0080) ? 0x02 : 0;

		if (sx >= 256) sx -= 512;

		if (data0 & 0x8000) continue;   /* disabled */

		/* Remap code 0x800 <-> 0x1000 */
		code = (code&0x7ff) | ((code&0x800)<<1) | ((code&0x1000)>>1);

		m_gfxdecode->gfx(3)->prio_transpen(bitmap,cliprect,
				code,
				color,
				flipx,flipy,
				sx,sy,
				screen.priority(),
				pri_mask,15);
	}
}



uint32_t bloodbro_state::screen_update_bloodbro(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	m_bg_tilemap->set_scrollx(0,m_scrollram[0]);
	m_bg_tilemap->set_scrolly(0,m_scrollram[1]);
	m_fg_tilemap->set_scrollx(0,m_scrollram[2]);
	m_fg_tilemap->set_scrolly(0,m_scrollram[3]);

	screen.priority().fill(0, cliprect);

	if(BIT(~m_layer_en, 0))
		m_bg_tilemap->draw(screen, bitmap, cliprect, 0,0);
	if(BIT(~m_layer_en, 1))
		m_fg_tilemap->draw(screen, bitmap, cliprect, 0,1);
	if(BIT(~m_layer_en, 4))
		m_spritegen->draw_sprites(screen, bitmap, cliprect, m_spriteram, m_spriteram.bytes());
	if(BIT(~m_layer_en, 3))
		m_tx_tilemap->draw(screen, bitmap, cliprect, 0,0);

	return 0;
}

uint32_t weststry_state::screen_update_weststry(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	// The bootleg video hardware probably also allows BG scrolling, but weststry doesn't use it
	m_fg_tilemap->set_scrollx(0, (int8_t)m_scrollram[1] - 13);
	m_fg_tilemap->set_scrolly(0, (int8_t)m_scrollram[0] + 1);

	screen.priority().fill(0, cliprect);

	m_bg_tilemap->draw(screen, bitmap, cliprect, 0,0);
	m_fg_tilemap->draw(screen, bitmap, cliprect, 0,1);
	weststry_draw_sprites(screen, bitmap,cliprect);
	m_tx_tilemap->draw(screen, bitmap, cliprect, 0,0);
	return 0;
}
