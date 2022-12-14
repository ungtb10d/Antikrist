# Copyright (C)  2012-2022   Mark Seligman
##
## This file is part of ArboristR.
##
## ArboristR is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## ArboristR is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with ArboristBridgeR.  If not, see <http://www.gnu.org/licenses/>.

# Pre-formats a data frame or buffer, if not already pre-formatted.
# If already pre-formatted, verifies types of member fields.
PreFormat <- function(x, verbose = FALSE) {
  warning("PreFormat is being deprecated.  Please use preformat() instead.");
    if (inherits(x, "Deframe")) {
        if (!inherits(x$rleFrame, "RLEFrame")) {
            stop("Missing RLEFrame")
        }
        if (verbose)
            print("Training set already pre-formatted")
        preFormat <- x
    }
    else {
        if (verbose)
            print("Blocking frame")

        if (verbose)
            print("Pre-sorting")

        preFormat <- deframe(x)
        if (verbose)
            print("Pre-formatting completed")
    }

    preFormat
}


