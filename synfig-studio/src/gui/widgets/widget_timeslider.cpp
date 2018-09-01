/* === S Y N F I G ========================================================= */
/*!	\file widget_timeslider.cpp
**	\brief Time Slider Widget Implementation File
**
**	$Id$
**
**	\legal
**	Copyright (c) 2004 Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2012, Carlos López
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <cmath>

#include <ETL/misc>

#include <synfig/general.h>

#include <gui/app.h>
#include <gui/helpers.h>

#include "widget_timeslider.h"

#include <gui/localization.h>

#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;

using studio::Widget_Timeslider;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */
const double zoominfactor = 0.75;
const double zoomoutfactor = 1/zoominfactor;

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

/* === E N T R Y P O I N T ================================================= */

const double defaultfps = 24;
const int fullheight = 20;

Widget_Timeslider::Widget_Timeslider():
	layout(Pango::Layout::create(get_pango_context())),
	adj_default(Gtk::Adjustment::create(0,0,2,1/defaultfps,10/defaultfps)),
	adj_timescale(),
	time_per_tickmark(0),
	//invalidated(false),
	last_event_time(0),
	fps(defaultfps),
	dragscroll(false),
	lastx(0)
{
	set_size_request(-1, fullheight);

	// click / scroll / zoom
	add_events(
		Gdk::BUTTON_PRESS_MASK   |
		Gdk::BUTTON_RELEASE_MASK |
		Gdk::BUTTON_MOTION_MASK  | 
		Gdk::SCROLL_MASK
	);

	set_time_adjustment(adj_default);
	//update_times();
}

Widget_Timeslider::~Widget_Timeslider()
{
}

void Widget_Timeslider::set_time_adjustment(const Glib::RefPtr<Gtk::Adjustment> &x)
{
	if (adj_timescale == x) return;

	//disconnect old connections
	time_value_change.disconnect();
	time_other_change.disconnect();

	//connect update function to new adjustment
	adj_timescale = x;

	if (adj_timescale) {
		time_value_change = adj_timescale->signal_value_changed().connect(sigc::mem_fun(*this,&Widget_Timeslider::queue_draw));
		time_other_change = adj_timescale->signal_changed().connect(sigc::mem_fun(*this,&Widget_Timeslider::queue_draw));
		//invalidated = true;
		//refresh();
	}
}

void Widget_Timeslider::set_global_fps(float d)
{
	if(fps != d)
	{
		fps = d;

		//update everything since we need to redraw already
		//invalidated = true;
		//refresh();
		queue_draw();
	}
}

/*void Widget_Timeslider::update_times()
{
	if(adj_timescale)
	{
		start = adj_timescale->get_lower();
		end = adj_timescale->get_upper();
		current = adj_timescale->get_value();
	}
}*/

void Widget_Timeslider::refresh()
{
}
/*
{
	if(invalidated)
	{
		queue_draw();
	}else if(adj_timescale)
	{
		double 	l = adj_timescale->get_lower(),
				u = adj_timescale->get_upper(),
				v = adj_timescale->get_value();

		bool invalid = (l != start) || (u != end) || (v != current);

		start = l;
		end = u;
		current = v;

		if(invalid) queue_draw();
	}
}*/

void Widget_Timeslider::draw_background(const Cairo::RefPtr<Cairo::Context> &cr)
{
	//draw grey rectangle
	cr->save();
	cr->set_source_rgb(0.5, 0.5, 0.5);
	cr->rectangle(0.0, 0.0, (double)get_width(), (double)get_height());
	cr->fill();
	cr->restore();
}

bool Widget_Timeslider::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
	const double EPSILON = 1e-6;

	draw_background(cr);

	if(!adj_timescale || get_width() == 0 || get_height() == 0) return true;

	//Get the time information since we now know it's valid
	double 	start   = adj_timescale->get_lower(),
			end     = adj_timescale->get_upper(),
			current = adj_timescale->get_value();

	if(end-start < EPSILON) return true;

	//draw all the time stuff
	double dtdp = (end - start)/get_width();
	double dpdt = 1/dtdp;

	//lines


	//Draw the time line...
	double tpx = round_to_int((current-start)*dpdt)+0.5;
	cr->save();
	cr->set_source_rgb(1.0, 175.0/255.0, 0.0);
	cr->set_line_width(1.0);
	cr->move_to(tpx, 0.0);
	cr->line_to(tpx, fullheight);
	cr->stroke();
	cr->restore();

	// Calculate the line intervals
	int ifps = round_to_int(fps);
	if (ifps < 1) ifps = 1;

	std::vector<double> ranges;

	//unsigned int pos = 0;

	// build a list of all the factors of the frame rate
	for (int i = 1, pos = 0; i*i <= ifps; i++)
		if ((ifps%i) == 0)
		{
			ranges.insert(ranges.begin()+pos, i/fps);
			if (i*i != ifps)
				ranges.insert(ranges.begin()+pos+1, ifps/i/fps);
			pos++;
		}

	// fill in any gaps where one factor is more than 2 times the previous
	std::vector<double>::iterator iter, next;
	
	for (unsigned int pos = 0; pos < ranges.size()-1; pos++)
	{
		iter = ranges.begin()+pos;
		next = iter+1;
		if (*iter*2 < *next)
			ranges.insert(next, *iter*2);
	}

	double more_ranges[] = {
		2, 3, 5, 10, 20, 30, 60, 90, 120, 180,
		300, 600, 1200, 1800, 2700, 3600, 3600*2,
		3600*4, 3600*8, 3600*16, 3600*32, 3600*64,
		3600*128, 3600*256, 3600*512, 3600*1024 };

	ranges.insert(ranges.end(), more_ranges, more_ranges + sizeof(more_ranges)/sizeof(double));

	double lowerrange = dtdp*140, upperrange = dtdp*280;
	double midrange = (lowerrange + upperrange)/2;

	//find most ideal scale
	double scale;
	next = binary_find(ranges.begin(), ranges.end(), midrange);
	iter = next++;

	if (iter == ranges.end()) iter--;
	if (next == ranges.end()) next--;

	if (abs(*next - midrange) < abs(*iter - midrange))
		iter = next;

	scale = *iter;

	// subdivide into this many tick marks (8 or less)
	int subdiv = round_to_int(scale * ifps);

	if (subdiv > 8)
	{
		const int ideal = subdiv;

		// find a number of tick marks that nicely divides the scale
		// (5 minutes divided by 6 is 50s, but that's not 'nice' -
		//  5 ticks of 1m each is much simpler than 6 ticks of 50s)
		for (subdiv = 8; subdiv > 0; subdiv--)
			if ((ideal <= ifps*2       && (ideal % (subdiv           )) == 0) ||
				(ideal <= ifps*2*60    && (ideal % (subdiv*ifps      )) == 0) ||
				(ideal <= ifps*2*60*60 && (ideal % (subdiv*ifps*60   )) == 0) ||
				(true                  && (ideal % (subdiv*ifps*60*60)) == 0))
				break;

		// if we didn't find anything, use 4 ticks
		if (!subdiv)
			subdiv = 4;
	}

	time_per_tickmark = scale / subdiv;

	//get first valid line and its position in pixel space
	double time = 0;
	double pixel = 0;

	int sdindex = 0;

	double subr = scale / subdiv;

	//get its position inside...
	time = ceil(start/subr)*subr - start;
	pixel = time*dpdt;

	//absolute time of the line to be drawn
	time += start;

	{ //inside the big'n
		double t = (time/scale - floor(time/scale))*subdiv; // the difference from the big mark in 0:1
		//sdindex = (int)floor(t + 0.5); //get how far through the range it is...
		sdindex = round_to_int(t); //get how far through the range it is...
		if (sdindex == subdiv) sdindex = 0;

		//synfig::info("Extracted fr %.2lf -> %d", t, sdindex);
	}

	//synfig::info("Initial values: %.4lf t, %.1lf pixels, %d i", time,pixel,sdindex);

	//loop to draw
	const double heightbig = 12;
	const double heightsmall = 4;

	// Draw the lines and timecode
	//normal line/text color
	cr->save();
	cr->set_source_rgb(51.0/255.0,51.0/255.0,51.0/255.0);
	cr->set_line_width(1.0);

	int width = get_width();
	while( pixel < width )
	{
		double xpx = round_to_int(pixel)+0.5;

		//draw big
		if(sdindex == 0)
		{
			cr->move_to(xpx,0);
			cr->line_to(xpx,heightbig);
			cr->stroke();
			//round the time to nearest frame and draw the text
			Time tm((double)time);
			if(get_global_fps()) tm.round(get_global_fps());
			Glib::ustring timecode(tm.get_string(get_global_fps(),App::get_time_format()));

			layout->set_text(timecode);
			Pango::AttrList attr_list;
			// Aproximately a font size of 8 pixels.
			// Pango::SCALE = 1024
			// create_attr_size waits a number in 1000th of pixels.
			// Should be user customizable in the future. Now it is fixed to 10
			Pango::AttrInt pango_size(Pango::Attribute::create_attr_size(Pango::SCALE*10));
			pango_size.set_start_index(0);
			pango_size.set_end_index(64);
			attr_list.change(pango_size);
			layout->set_attributes(attr_list);
			cr->move_to(xpx+1.0,0);
			layout->show_in_cairo_context(cr);
		}else
		{
			cr->move_to(xpx,0);
			cr->line_to(xpx,heightsmall);
			cr->stroke();
		}

		//increment time and position
		pixel += subr / dtdp;
		time += subr;

		//increment index
		if(++sdindex >= subdiv) sdindex -= subdiv;
	}
	cr->restore();

	//Draw the time line afer all
	Gdk::RGBA c("#ffaf00");
	cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
	cr->set_line_width(3);
	tpx = (current-start)*dpdt;
	cr->move_to(round_to_int(tpx),0);
	cr->line_to(round_to_int(tpx),fullheight);
	cr->stroke();

	return true;
}

bool Widget_Timeslider::on_motion_notify_event(GdkEventMotion* event) //for dragging
{
	if(!adj_timescale) return false;

	Gdk::ModifierType mod = Gdk::ModifierType(event->state);

	//scrolling...

	//NOTE: we might want to address the possibility of dragging with both buttons held down

	if(mod & Gdk::BUTTON2_MASK)
	{

		//we need this for scrolling by dragging
		double 	curx = event->x;

		double 	start = adj_timescale->get_lower(),
				end = adj_timescale->get_upper();

		if(dragscroll)
		{
			if(event->time-last_event_time<30)
				return false;
			else
				last_event_time=event->time;

			if(abs(lastx - curx) < 1 && end != start) return true;
			//translate the window and correct it

			//update our stuff so we are operating correctly
			//invalidated = true;
			//update_times();

			//Note: Use inverse of mouse movement because of conceptual space relationship
			double diff = lastx - curx; //curx - lastx;

			//NOTE: This might be incorrect...
			//fraction to move...
			double dpx = (end - start)/get_width();
			lastx = curx;

			diff *= dpx;

			//Adjust...
			start += diff;
			end += diff;

			//But clamp to bounds if they exist...
			//HACK - bounds should not be required for this slider
			if (adj_bounds) {
				if(start < adj_bounds->get_lower()) {
					diff = adj_bounds->get_lower() - start;
					start += diff;
					end += diff;
				}
				if(end > adj_bounds->get_upper()) {
					diff = adj_bounds->get_upper() - end;
					start += diff;
					end += diff;
				}
			}

			//synfig::info("Scrolling timerange to (%.4f,%.4f)",start,end);

			ConfigureAdjustment(adj_timescale)
				.set_lower(start)
				.set_upper(end)
				.finish();
		}else
		{
			dragscroll = true;
			lastx = curx;
			//lasty = cury;
		}

		return true;
	}

	if(mod & Gdk::BUTTON1_MASK)
	{
		double curx = event->x;

		//get time from drag...
		double start = adj_timescale->get_lower();
		double end = adj_timescale->get_upper();
		double current = adj_timescale->get_value();
		Time t(start + curx*(end - start)/get_width());

		//snap it to fps - if they exist...
		if (fps) t = t.round(fps);

		//set time if needed
		if (current != t)
			adj_timescale->set_value((double)t);

		return true;
	}

	return false;
}

bool Widget_Timeslider::on_scroll_event(GdkEventScroll* event) //for zooming
{
	if(!adj_timescale) return false;

	//Update so we are calculating based on current values
	//update_times();

	//figure out if we should center ourselves on the current time
	bool center = false;

	//we want to zoom in on the time value if control is held down
	if(Gdk::ModifierType(event->state) & Gdk::CONTROL_MASK)
		center = true;

	switch(event->direction)
	{
		case GDK_SCROLL_UP: //zoom in
			zoom_in(center);
			return true;

		case GDK_SCROLL_DOWN: //zoom out
			zoom_out(center);
			return true;

		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_LEFT:
		{
			double t = adj_timescale->get_value();
			double orig_t = t;
			double start = adj_timescale->get_lower();
			double end = adj_timescale->get_upper();
			double lower = adj_bounds->get_lower();
			double upper = adj_bounds->get_upper();
			double adj = time_per_tickmark;

			if( event->direction == GDK_SCROLL_RIGHT )
			{
				// step forward one tick
				t += adj;

				// don't go past the end of time
				if (t > upper)
					t = upper;

				// if we are already in the right half of the slider
				if ((t-start)*2 > (end-start)) {
					// if we can't scroll the background left one whole tick, scroll it to the end
					if (end > upper - (t-orig_t)) {
						ConfigureAdjustment(adj_timescale)
							.set_lower(upper - (end-start))
							.set_upper(upper)
							.finish();
					} else {
						// else scroll the background left
						ConfigureAdjustment(adj_timescale)
							.set_lower(start + (t-orig_t))
							.set_upper(start + (t-orig_t) + (end-start))
							.finish();
					}
				}
			}
			else
			{
				// step backwards one tick
				t -= adj;

				// don't go past the start of time
				if (t < lower)
					t = lower;

				// if we are already in the left half of the slider
				if ((t-start)*2 < (end-start))
				{
					// if we can't scroll the background right one whole tick, scroll it to the beginning
					if (start < lower + (orig_t-t)) {
						ConfigureAdjustment(adj_timescale)
							.set_lower(lower)
							.set_upper(lower + (end-start))
							.finish();
					} else {
						// else scroll the background left
						ConfigureAdjustment(adj_timescale)
							.set_lower(start - (orig_t-t))
							.set_upper(start - (orig_t-t) + (end-start))
							.finish();
					}
				}
			}

			if (adj_timescale)
				adj_timescale->set_value(t);

			return true;
		}
		default:
			return false;
	}
}

void Widget_Timeslider::zoom_in(bool centerontime)
{
	if(!adj_timescale) return;

	double 	start = adj_timescale->get_lower(),
			end = adj_timescale->get_upper(),
			current = adj_timescale->get_value();

	double focuspoint = centerontime ? current : (start + end)/2;

	//calculate new beginning and end
	end = focuspoint + (end-focuspoint)*zoominfactor;
	start = focuspoint + (start-focuspoint)*zoominfactor;

	//synfig::info("Zooming in timerange to (%.4f,%.4f)",start,end);
	if (adj_bounds) {
		if(start < adj_bounds->get_lower())
			start = adj_bounds->get_lower();
		if(end > adj_bounds->get_upper())
			end = adj_bounds->get_upper();
	}

	//reset values
	ConfigureAdjustment(adj_timescale)
		.set_lower(start)
		.set_upper(end)
		.finish();
}

void Widget_Timeslider::zoom_out(bool centerontime)
{
	if(!adj_timescale) return;

	double 	start = adj_timescale->get_lower(),
			end = adj_timescale->get_upper(),
			current = adj_timescale->get_value();

	double focuspoint = centerontime ? current : (start + end)/2;

	//calculate new beginning and end
	end = focuspoint + (end-focuspoint)*zoomoutfactor;
	start = focuspoint + (start-focuspoint)*zoomoutfactor;

	//synfig::info("Zooming out timerange to (%.4f,%.4f)",start,end);
	if(adj_bounds) {
		if(start < adj_bounds->get_lower())
			start = adj_bounds->get_lower();
		if(end > adj_bounds->get_upper())
			end = adj_bounds->get_upper();
	}

	//reset values
	ConfigureAdjustment(adj_timescale)
		.set_lower(start)
		.set_upper(end)
		.finish();
}

bool Widget_Timeslider::on_button_press_event(GdkEventButton *event) //for clicking
{
	switch(event->button)
	{
		//time click...
		case 1:
		{
			double start   = adj_timescale->get_lower();
			double end     = adj_timescale->get_upper();
			double w = get_width();
			Time t(start + (end - start)*event->x/w);
			if (fps) t = t.round(fps);

			/*synfig::info("Clicking time from %.3lf to %.3lf [(%.2lf,%.2lf) %.2lf / %.2lf ... %.2lf",
						current, vt, start, end, event->x, w, fps);*/

			if(adj_timescale)
				adj_timescale->set_value((double)t);

			break;
		}

		//scroll click
		case 2:
		{
			//start dragging
			dragscroll = true;
			lastx = event->x;
			//lasty = event->y;

			return true;
		}

		default:
		{
			break;
		}
	}

	return false;
}

bool Widget_Timeslider::on_button_release_event(GdkEventButton *event) //end drag
{
	switch(event->button)
	{
		case 2:
		{
			//start dragging
			dragscroll = false;
			return true;
		}

		default:
		{
			break;
		}
	}

	return false;
}
