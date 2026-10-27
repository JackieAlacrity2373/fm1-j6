"""Dev shim: Pillow without libraqm rejects features=...; drop it (only tabular digits differ)."""
from PIL import ImageFont, ImageDraw, features

if not features.check("raqm"):
    _bbox, _len = ImageFont.FreeTypeFont.getbbox, ImageFont.FreeTypeFont.getlength
    _text = ImageDraw.ImageDraw.text

    def getbbox(self, *a, **k):
        k.pop("features", None)
        return _bbox(self, *a, **k)

    def getlength(self, *a, **k):
        k.pop("features", None)
        return _len(self, *a, **k)

    def text(self, *a, **k):
        k.pop("features", None)
        return _text(self, *a, **k)

    ImageFont.FreeTypeFont.getbbox = getbbox
    ImageFont.FreeTypeFont.getlength = getlength
    ImageDraw.ImageDraw.text = text
