
/* Volume Icon */
/*
 *  __/|
 * |\/ |
 * |/\ |
 * ~~~\|
 */
XPoint vol_points_mute_body[] = {{0,1},{0,2},{1,2},{2,3},{2,0},{1,1},{0,1},{-1,-1}}; /* Mute */
XPoint vol_points_mute_x1[] = {{0,0},{2,3},{-1,-1}};
XPoint vol_points_mute_x2[] = {{2,0},{0,3},{-1,-1}};
XArc vol_arcs_mute_body[] = {{0,0,2,3,0,360*64},{-1,-1,0,0,0,0}};

x_atom vol_points_mute[] = {
	{.is_fill = 1, .member = (x_member)vol_points_mute_body},
	{.is_fill = 0, .member = (x_member)vol_points_mute_x1},
	{.is_fill = 0, .member = (x_member)vol_points_mute_x2},
	{.member = NULL},
};
x_atom vol_arcs_mute[] = {
	{.is_fill = 0, .member = (x_member)vol_arcs_mute_body},
	{.member = NULL},
};
/*
 *  __/|
 * |   |
 * |   |
 * ~~~\|
 */
XPoint vol_points_normal_body[] = {{0,1},{0,2},{1,2},{2,3},{2,0},{1,1},{0,1},{-1,-1}}; /* Normal */
x_atom vol_points_normal[] = {
	{.is_fill = 1, .member = (x_member)vol_points_normal_body},
	{.member = NULL},
};
/* volume Icon end */


/* Brightness Icon */
XPoint brt_points_1[] = {{3,3},{13,13},{-1,-1}};
XPoint brt_points_2[] = {{8,1},{8,15},{-1,-1}};
XPoint brt_points_3[] = {{13,3},{3,13},{-1,-1}};
XPoint brt_points_4[] = {{15,8},{1,8},{-1,-1}};

x_atom brt_points[] = {
	{.is_fill = 0, .member = (x_member)brt_points_1},
	{.is_fill = 0, .member = (x_member)brt_points_2},
	{.is_fill = 0, .member = (x_member)brt_points_3},
	{.is_fill = 0, .member = (x_member)brt_points_4},
	{.member = NULL},
};

XArc brt_arcs_body[] = {{4,4,8,8,0,360*64},{-1,-1,0,0,0,0}};
x_atom brt_arcs[] = {
	{.is_fill = 1, .member = (x_member)brt_arcs_body},
	{.member = NULL},
};
/* brightness Icon end */


xosd_icon_element xosd_icon_element_volume[] = {
	{
		.bar = 1, /* Normal */
		.atom = 
		{
			{
				.x_atom = NULL,
			},
			{
				.atom_type = ATOM_TYPE_point,
				.x_atom = vol_points_normal,
			},
			{
				.x_atom = NULL,
			},
		},
	},
	{
		.bar = 0, /* Mute */
		.atom = 
		{
			{
				.x_atom = NULL,
			},
			{
				.atom_type = ATOM_TYPE_point,
				.x_atom = vol_points_mute,
			},
			{
				.atom_type = ATOM_TYPE_arc,
				.x_atom = vol_arcs_mute,
			},
		},
	}
};

xosd_icon_element xosd_icon_element_brightness[] = {
	{
		.bar = 0, /* Normal */
		.atom =
		{
			{
				.x_atom = NULL
			},
			{
				.atom_type = ATOM_TYPE_point,
				.x_atom = brt_points,
			},
			{
				.atom_type = ATOM_TYPE_arc,
				.x_atom = brt_arcs,
			}
		},
	},
};

xosd_icon osd_icon[] = {
	{
		.type = XOSD_volume,
		.nstage = 2,
		.block = 3,
		.element = xosd_icon_element_volume,
	},
	{
		.type = XOSD_brightness,
		.nstage = 1,
		.block = 16,
		.element = xosd_icon_element_brightness,
	},
	{
		.type = XOSD_null,
	}
};

