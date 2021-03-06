import wx
from constantTable import ConstantTable

class GCPath(object):
    ''' an object encapsulating a wxGraphicsPath '''
    # implements(IPath)
    
    def __init__(self, renderer):
        self.renderer = renderer
        self.path = renderer.wx_renderer.CreatePath()
        
    def Render(self, fillStyle = 'oddeven'):
        ''' Renders the path with the given fillstyle (one of 'line', 'fill' or
            'fill_and_line')
        '''
        fill_mode = self.renderer.fill_mode
        if fill_mode == 'line':
            self.Stroke()
        elif fill_mode == 'fill':
            self.Fill()
        elif fill_mode == 'fill_and_line':
            self.FillAndStroke()
        else:
            raise ValueError(' invalid fill_mode %s' % fill_mode )

    def FillAndStroke(self, fillStyle = 'oddeven'):
        return self.renderer.GC.DrawPath(self.path, ConstantTable.get(fillStyle) )
    
    def Fill(self, fillStyle = 'oddeven'):
        return self.renderer.GC.FillPath(self.path, ConstantTable.get(fillStyle) )    

    def Stroke(self):
        return self.renderer.GC.StrokePath(self.path)

    # maps all the other functions
    def __getattr__(self, name):
        return getattr(self.path, name)


class GCFont(object):
    ''' A wrapper around a wxGraphicsFont font '''
    def __init__(self, renderer, size, family, style, weight, underlined, faceName, colour):
        self.renderer = renderer
        self.size = size
        self.family = family
        self.style = style
        self.weight = weight
        self.underlined = underlined
        self.faceName = faceName
        self.colour = colour
        self.create()
        
    def create(self):
        family = ConstantTable.getEnum( 'fontfamily', self.family)
        style = ConstantTable.getEnum( 'fontstyle', self.style)
        weight = ConstantTable.getEnum( 'fontweight', self.weight)
        font = wx.Font( self.size, family, style, weight, self.underlined, self.faceName )
        self.font = self.renderer.wx_renderer.CreateFont( font, self.colour )

    def Activate(self):
        #if self.renderer.active_font == self:
        #    return
        self.renderer.active_font = self
        try:
            gc = self.renderer.GC
        except AttributeError:  # this can happen if there's no active gc
            pass
        else:
            gc.SetFont(self.font)
            
        # always inform the measuring context about the font
        self.renderer.measuringContext.SetFont( self.font )


class GCBrush(object):
    ''' A wrapper around a wxGraphicsBrush brush '''
    def __init__(self, renderer, kind, *args, **keys):
        self.renderer = renderer
        wx_renderer = renderer.wx_renderer
        self.kind = kind
        if kind == 'plain':
            brush = wx_renderer.CreateBrush( wx.Brush( *args, **keys ) )
        elif kind == 'linearGradient':
            brush = wx_renderer.CreateLinearGradientBrush( *args, **keys )
        elif kind == 'radialGradient':
            brush = wx_renderer.CreateRadialGradientBrush( *args, **keys )
        elif kind is None:
            brush = wx_renderer.CreateBrush( wx.NullBrush )
        else:
            raise ValueError('Wrong kind for brush %s' % kind)

        self.brush = brush

    def Activate(self):
        #if self.renderer.active_brush == self:
        #    return
        self.renderer.active_brush = self
        try:
            gc = self.renderer.GC
        except AttributeError:  # this can happen if there's no active gc
            pass
        else:
            gc.SetBrush(self.brush)

class GCPen(object):
    ''' A wrapper around a wxGraphicsPen pen '''
    def __init__(self, renderer, colour, width, style, cap, join, dashes, stipple, wx_pen):
        self.renderer = renderer
        self.colour = colour
        self.width = width
        self.style = style
        self.cap = cap
        self.join = join
        self.dashes = dashes
        self.stipple = stipple
        self.wx_pen = wx_pen
        self.create()
        
    def create(self):
        if self.wx_pen is not None:
            pen = self.wx_pen
        else:
            pen = wx.Pen( self.colour, self.width, ConstantTable.getValue( self.style ) )
            
        pen.SetCap( ConstantTable.getEnum( 'cap', self.cap) )
        pen.SetJoin( ConstantTable.getEnum( 'join', self.join) )
        if self.dashes is not None:
            pen.SetDashes( self.dashes )
        if self.stipple is not None:
            pen.SetStipple( self.stipple )
            
        self.pen = self.renderer.wx_renderer.CreatePen( pen )

    def Activate(self):
        #if self.renderer.active_pen == self:
        #    return
        self.renderer.active_pen = self
        try:
            gc = self.renderer.GC
        except AttributeError:  # this can happen if there's no active gc
            pass
        else:
            gc.SetPen( self.pen )
